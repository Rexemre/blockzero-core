// Copyright (c) 2026 The Block Zero developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Block Zero genesis miner.
//
// Reproducibly mines genesis blocks under the RandomX proof-of-work using the
// same light-mode hashing the node uses (RandomXComputeHash / RandomXBootstrapKey).
// Multi-threaded; serializes the 80-byte header once per thread and only patches
// the nonce bytes per attempt. Prints the values needed for kernel/chainparams.cpp.

#include <arith_uint256.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <pow.h>
#include <pow_randomx.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <streams.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.version = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

void MineOne(const char* net, const char* pszTimestamp, uint32_t nTime, uint32_t nBits)
{
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;

    auto target = DeriveTarget(nBits, uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"});
    if (!target) { std::printf("[%s] invalid nBits\n", net); return; }

    const uint256 key = RandomXBootstrapKey();
    unsigned nthreads = std::thread::hardware_concurrency();
    if (nthreads == 0) nthreads = 4;

    std::printf("[%s] mining (nBits=0x%08x, nTime=%u) with %u threads (light mode) ...\n", net, nBits, nTime, nthreads);
    std::fflush(stdout);

    std::atomic<bool> found{false};
    std::atomic<uint32_t> found_nonce{0};
    auto t0 = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (unsigned t = 0; t < nthreads; ++t) {
        threads.emplace_back([&, t]() {
            const arith_uint256 tgt = *target;
            // Serialize the 80-byte header once; only the nonce (last 4 bytes) changes.
            CBlock g = CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, 0, nBits, 1, 50 * COIN);
            DataStream ss;
            ss << static_cast<const CBlockHeader&>(g);
            unsigned char* bytes = reinterpret_cast<unsigned char*>(ss.data());
            const size_t len = ss.size(); // 80
            for (uint64_t nonce = t; nonce <= 0xffffffffULL; nonce += nthreads) {
                if ((nonce & 0x3ff) == t && found.load(std::memory_order_relaxed)) return;
                const uint32_t n = (uint32_t)nonce;
                std::memcpy(bytes + len - 4, &n, 4); // patch nNonce (LE)
                if (UintToArith256(RandomXComputeHash(key, std::span<const unsigned char>{bytes, len})) <= tgt) {
                    bool expected = false;
                    if (found.compare_exchange_strong(expected, true)) found_nonce.store(n);
                    return;
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    if (!found.load()) { std::printf("[%s] no nonce found in range\n", net); return; }

    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    CBlock genesis = CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, found_nonce.load(), nBits, 1, 50 * COIN);
    std::printf("[%s] FOUND in %.1fs\n", net, secs);
    std::printf("  nNonce        = %u\n", found_nonce.load());
    std::printf("  nTime         = %u\n", nTime);
    std::printf("  nBits         = 0x%08x\n", nBits);
    std::printf("  powHash       = %s\n", GetBlockPoWHash(genesis, key).GetHex().c_str());
    std::printf("  hashGenesis   = %s\n", genesis.GetHash().GetHex().c_str());
    std::printf("  hashMerkleRoot= %s\n", genesis.hashMerkleRoot.GetHex().c_str());
    std::fflush(stdout);
}

} // namespace

int main()
{
    // Testnet genesis (2026-06-04), already frozen in chainparams.
    const char* testnet_msg =
        "The Times 04/Jun/2026 Block Zero - a second chance at Genesis";
    MineOne("testnet", testnet_msg, 1780531200, 0x1e3fffff);

    // Mainnet genesis - launch 2026-06-06 at 06:06:06 UTC (1780725966).
    // The future timestamp also gates block 1: no block can be mined before launch.
    const char* mainnet_msg =
        "The Times 06/Jun/2026 Block Zero - a second chance at Genesis";
    MineOne("mainnet", mainnet_msg, 1780725966, 0x1e3fffff);

    return 0;
}
