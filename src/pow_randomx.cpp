// Copyright (c) 2026 The Block Zero developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow_randomx.h>

#include <logging.h>
#include <randomx/src/randomx.h>

#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#ifdef WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#else
#include <fstream>
#include <string>
#endif

namespace {

// Fast mode needs a ~2080 MiB dataset plus cache and headroom. On a box with
// less than this, allocating/initializing the dataset thrashes swap and stalls
// mining instead of helping, so we skip it and mine in light mode (256 MiB).
// Light and fast modes compute identical hashes, so this is purely a
// performance/stability choice and never affects consensus.
constexpr uint64_t kFastModeMinMiB = 3072;

// Physically available RAM in MiB, or 0 when it cannot be determined.
uint64_t AvailableMemoryMiB()
{
#ifdef WIN32
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st)) {
        return static_cast<uint64_t>(st.ullAvailPhys) / (1024ULL * 1024ULL);
    }
    return 0;
#elif defined(__APPLE__)
    uint64_t total = 0;
    size_t len = sizeof(total);
    if (sysctlbyname("hw.memsize", &total, &len, nullptr, 0) == 0) {
        return total / (1024ULL * 1024ULL);
    }
    return 0;
#else
    std::ifstream meminfo("/proc/meminfo");
    std::string field, unit;
    uint64_t value = 0;
    while (meminfo >> field >> value >> unit) {
        if (field == "MemAvailable:") return value / 1024ULL; // kB -> MiB
    }
    return 0;
#endif
}

// Per-thread RandomX state. RandomX VMs are not safe to share between threads,
// so each thread keeps its own cache + VM. The cache is rebuilt only when the
// key changes, which by design happens rarely (seed rotation epochs).
struct ThreadRandomX {
    randomx_cache* cache{nullptr};
    randomx_vm* vm{nullptr};
    uint256 current_key;
    bool initialized{false};

    ~ThreadRandomX()
    {
        if (vm) randomx_destroy_vm(vm);
        if (cache) randomx_release_cache(cache);
    }

    void EnsureKey(const uint256& key)
    {
        if (initialized && current_key == key) return;

        const randomx_flags flags = randomx_get_flags();

        if (!cache) {
            cache = randomx_alloc_cache(flags);
        }
        randomx_init_cache(cache, key.data(), key.size());

        if (vm) {
            randomx_vm_set_cache(vm, cache);
        } else {
            vm = randomx_create_vm(flags, cache, nullptr);
        }

        current_key = key;
        initialized = true;
    }
};

thread_local ThreadRandomX g_thread_randomx;

// Shared fast-mode dataset (~2 GiB) used by all mining threads. Built once per
// key under g_mining.mtx; read lock-free during the grind because it is stable
// for the duration of a mining round (rebuilt only on key rotation).
struct MiningDataset {
    std::mutex mtx;
    randomx_dataset* dataset{nullptr};
    randomx_flags vm_flags{};
    uint256 key;
    bool ready{false};
};

MiningDataset g_mining;

} // namespace

uint256 RandomXComputeHash(const uint256& key, std::span<const unsigned char> input)
{
    g_thread_randomx.EnsureKey(key);

    unsigned char output[RANDOMX_HASH_SIZE];
    randomx_calculate_hash(g_thread_randomx.vm, input.data(), input.size(), output);

    uint256 result;
    std::memcpy(result.data(), output, result.size());
    return result;
}

bool RandomXInitMiningDataset(const uint256& key, unsigned init_threads)
{
    std::lock_guard<std::mutex> lock(g_mining.mtx);
    if (g_mining.ready && g_mining.key == key) return true;

    // Don't even attempt the 2 GiB dataset when RAM is short - it would thrash
    // swap and stall mining. Light mode (identical hashes) is used instead.
    const uint64_t avail = AvailableMemoryMiB();
    if (avail > 0 && avail < kFastModeMinMiB) {
        LogInfo("RandomX mining: only %d MiB RAM available, using light mode "
                "(fast mode needs ~%d MiB)\n", avail, kFastModeMinMiB);
        return false;
    }

    const randomx_flags base = randomx_get_flags();
    const randomx_flags vm_flags = base | RANDOMX_FLAG_FULL_MEM;

    randomx_cache* cache = randomx_alloc_cache(base);
    if (!cache) return false;
    randomx_init_cache(cache, key.data(), key.size());

    if (!g_mining.dataset) {
        g_mining.dataset = randomx_alloc_dataset(vm_flags);
        if (!g_mining.dataset) {
            randomx_release_cache(cache);
            return false;
        }
    }

    const unsigned long total = randomx_dataset_item_count();
    unsigned nthreads = init_threads ? init_threads : 1;
    if (nthreads > total) nthreads = 1;
    const unsigned long per = total / nthreads;

    std::vector<std::thread> workers;
    workers.reserve(nthreads);
    for (unsigned i = 0; i < nthreads; ++i) {
        const unsigned long start = static_cast<unsigned long>(i) * per;
        const unsigned long count = (i == nthreads - 1) ? (total - start) : per;
        randomx_dataset* ds = g_mining.dataset;
        workers.emplace_back([ds, cache, start, count]() {
            randomx_init_dataset(ds, cache, start, count);
        });
    }
    for (auto& w : workers) w.join();

    randomx_release_cache(cache);

    g_mining.vm_flags = vm_flags;
    g_mining.key = key;
    g_mining.ready = true;
    return true;
}

RandomXFastHasher::RandomXFastHasher(const uint256& key)
    : m_key(key)
{
    randomx_dataset* ds = nullptr;
    randomx_flags flags{};
    {
        std::lock_guard<std::mutex> lock(g_mining.mtx);
        if (g_mining.ready && g_mining.key == key) {
            ds = g_mining.dataset;
            flags = g_mining.vm_flags;
        }
    }
    if (ds) {
        m_vm = randomx_create_vm(flags, nullptr, ds);
    }
}

RandomXFastHasher::~RandomXFastHasher()
{
    if (m_vm) randomx_destroy_vm(static_cast<randomx_vm*>(m_vm));
}

uint256 RandomXFastHasher::Hash(std::span<const unsigned char> input) const
{
    if (!m_vm) {
        // Fast mode unavailable; fall back to light mode so mining still works.
        return RandomXComputeHash(m_key, input);
    }

    unsigned char output[RANDOMX_HASH_SIZE];
    randomx_calculate_hash(static_cast<randomx_vm*>(m_vm), input.data(), input.size(), output);

    uint256 result;
    std::memcpy(result.data(), output, result.size());
    return result;
}

uint256 RandomXBootstrapKey()
{
    // Deterministic, public bootstrap key for the early chain. No secrets.
    return uint256{"426c6f636b5a65726f2d52616e646f6d582d626f6f7473747261702d6b657976"};
}
