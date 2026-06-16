# BLOCK ZERO

### **Mine with your CPU** — not a warehouse. **Start at Block Zero.**

**RandomX · CPU-only (no ASIC farms, no GPU edge) · Fair launch · Pure proof-of-work · No presale · No insiders · No premine**

Modern **Bitcoin Core v31**. **RandomX** runs on ordinary processors — laptop, desktop, home server — not industrial mining hardware.

**👉 New here? [Join Discord →](https://discord.gg/FbJzrwAU2W)** · **🌐 [bloz.org](https://bloz.org)** · **⛏ [pool.bloz.org](https://pool.bloz.org)** · **🔍 [explorer.bloz.org](https://explorer.bloz.org)**

> A second chance at **Bitcoin-style home mining**: open your wallet, point your
> **CPU** at the network, and earn blocks. Same battle-tested Bitcoin Core engine —
> but **RandomX** keeps **ASIC warehouses and GPU farms out**, so miners are people
> with normal PCs, not datacenters.

**Mainnet live** since 2026-06-06 · height **1726+** · seed `217.160.46.61:8210`

---

## Why Block Zero (in 30 seconds)

| | **Block Zero** | Typical altcoin |
|---|---|---|
| **Launch** | Fair — mine from genesis | Presale, team allocation, VC |
| **Mining** | Your **CPU only** — RandomX blocks ASICs & GPUs | ASIC farms or GPU pools |
| **Codebase** | Modern **Bitcoin Core v31** | Fork-and-forget spaghetti |
| **Insiders** | **None** | Founders, advisors, VCs |

---

## Start in 3 steps

1. **Wallet (Windows)** — download **[v1.0.0-rc24](https://github.com/Rexemre/blockzero-core/releases/tag/v1.0.0-rc24)** (`blockzero-v1.0.0-rc24-windows-x64.zip`), unzip, run `bin\bitcoin-qt.exe`. MSVC runtime is included — no extra install needed.
2. **Mine on the official pool** — get a `bz1…` address from the wallet, then:
   ```bash
   git clone https://github.com/Rexemre/blockzero-ops.git
   cd blockzero-ops/scripts/mainnet
   ./mine-pool.sh bz1YOURADDRESS          # Linux / macOS
   ```
   Windows: `.\install-windows.ps1` then `.\mine-mainnet.ps1 -Pool` — see [blockzero-ops](https://github.com/Rexemre/blockzero-ops).
3. **Stuck or curious?** — **[Join Discord](https://discord.gg/FbJzrwAU2W)** (fastest help). Check your stats at [pool.bloz.org](https://pool.bloz.org) with your `bz1` address.

*You should see `New job: … - mining.` and hashrate > 0 within a minute. Pool miner: [pool-miner-v0.6.9](https://github.com/Rexemre/blockzero-ops/releases/tag/pool-miner-v0.6.9).*

---

## What Block Zero is

Block Zero is an **independent layer-1 blockchain** built on the **Bitcoin Core v31**
codebase. It keeps everything that makes Bitcoin solid — the UTXO model, script,
SegWit, Taproot, the wallet, the P2P network — and changes exactly one thing that
matters for fairness: **how blocks are mined**.

Instead of SHA-256 (which today only specialized ASIC machines can mine
profitably), Block Zero uses **RandomX** proof-of-work — the same CPU-optimized
algorithm used by Monero. RandomX is built to run fast on a normal computer's CPU
and *badly* on ASICs and GPUs. The result:

- **Your everyday PC is competitive again.** No mining rig, no datacenter.
- **No head start for anyone.** The chain was launched and mined openly from block 1 — no premine, no presale, no team allocation.
- **Home mining again.** Download the node, start mining, earn blocks — when a normal computer was enough, before hashrate belonged to warehouses.

This is **not** Bitcoin and not a token on someone else's chain. It is a fresh
chain with its own genesis block, its own rules, and its own coin: **BLOZ**.

---

## Why ASICs (and GPU farms) have no chance

| | SHA-256 (Bitcoin today) | RandomX (Block Zero) |
|---|---|---|
| Best hardware | Purpose-built **ASIC** machines | A normal **CPU** |
| Who can mine | Industrial farms | Anyone with a computer |
| ASIC advantage | Enormous | Practically none — RandomX needs a CPU's large cache, fast memory and general-purpose instructions, which ASICs can't cheaply replicate |
| GPU advantage | High | Low — RandomX is memory-hard and branch-heavy, hostile to GPUs |

RandomX continuously executes randomized programs that depend on a multi-megabyte
dataset in fast memory. A general-purpose CPU does this naturally; building an ASIC
for it would essentially mean building a CPU. **That is the whole point** —
mining stays in the hands of ordinary people.

---

## Official links

| | |
|---|---|
| **Website** | https://bloz.org |
| **Pool** | https://pool.bloz.org |
| **Explorer** | https://explorer.bloz.org |
| **Bridge** | https://bridge.bloz.org |
| **Discord** | https://discord.gg/FbJzrwAU2W |
| **X (Twitter)** | https://x.com/Block_Zero_2009 |
| **Full list** | [official-links.md](https://github.com/Rexemre/blockzero-docs/blob/main/official-links.md) |

---

## Chain parameters

| Property | Value |
|---|---|
| **Consensus engine** | Bitcoin Core v31 (UTXO, SegWit, Taproot) |
| **Proof-of-work** | RandomX (CPU-friendly, ASIC/GPU-resistant) |
| **Block time** | 10 minutes (target) |
| **Difficulty retarget** | every **72 blocks** (~12 hours) — adapts quickly to hashrate |
| **Initial block reward** | **50 BLOZ** |
| **Halving** | every **210,000 blocks** (~4 years) |
| **Maximum supply** | **21,000,000 BLOZ** |
| **Coinbase maturity** | 100 blocks |
| **Smallest unit** | 1 BLOZ = 100,000,000 base units (8 decimals) |
| **Ticker** | **BLOZ** (mainnet) · **TBLOZ** (testnet) |
| **Address prefix (bech32)** | `bz` (mainnet) · `tbz` (testnet) |
| **P2P port** | 8210 (mainnet) · 18210 (testnet) |
| **RPC port** | 8211 (mainnet) · 18211 (testnet) |

### Emission

Block Zero mirrors Bitcoin's disciplined, predictable issuance: a fixed
**21,000,000 BLOZ** cap, reached through halvings. Every 210,000 blocks the block
reward halves (50 → 25 → 12.5 → …), so the supply curve and scarcity behave exactly
like Bitcoin's — only the mining hardware is different.

### Genesis

- **Mainnet genesis:** `44c1a8c852b3eda21966e1ddb6b0807e22488dffe8a270bf24bf1fa2d66c13bd` (launch 2026-06-06 06:06:06 UTC)
- **Testnet genesis:** `7462293eec16a92c54a74362af6825688135e2955250024dcc3668ff4f55cfce` (see [artifacts/genesis/testnet.json](artifacts/genesis/testnet.json))

---

## Start mining (mainnet)

Mine **BLOZ** on the live mainnet — your CPU, your blocks.

**Prebuilt binaries:** [Releases](https://github.com/Rexemre/blockzero-core/releases)
(node `bitcoind`, CLI `bitcoin-cli`, and the GUI wallet `bitcoin-qt`)

**One-click setup (Windows):**

```powershell
git clone https://github.com/Rexemre/blockzero-ops.git
cd blockzero-ops\scripts\mainnet
.\install-windows.ps1
.\mine-mainnet.ps1 -Status   # sync to the public seed first
.\mine-mainnet.ps1 -Pool     # pool mine (recommended)
```

**Public seed:** `217.160.46.61:8210` · **Block explorer:** https://explorer.bloz.org

**GUI wallet (`bitcoin-qt`):** download from [Releases](https://github.com/Rexemre/blockzero-core/releases). Latest Windows build: **v1.0.0-rc24** (bundled MSVC runtime). v1.0.0-rc10+ embeds the seed as a fixed peer; on older builds add `addnode=217.160.46.61:8210` to `%LOCALAPPDATA%\BlockZeroMainnet\bitcoin.conf` — see [blockzero-wallet](https://github.com/Rexemre/blockzero-wallet).

Testnet (TBLOZ) remains available for development — see [blockzero-docs/quickstart-mining.md](https://github.com/Rexemre/blockzero-docs/blob/main/quickstart-mining.md#testnet-optional).

---

## Build from source

```bash
git clone --recurse-submodules https://github.com/Rexemre/blockzero-core.git
cd blockzero-core
cmake -B build
cmake --build build -j$(nproc) --target bitcoind bitcoin-cli bitcoin-qt
```

See [doc/build-unix.md](doc/build-unix.md) and [doc/build-windows-msvc.md](doc/build-windows-msvc.md).

---

## Repositories

| Repo | Purpose |
|------|---------|
| **blockzero-core** (here) | Node, consensus, RandomX proof-of-work |
| [blockzero-docs](https://github.com/Rexemre/blockzero-docs) | Guides, specs, status |
| [blockzero-ops](https://github.com/Rexemre/blockzero-ops) | Mining scripts, seed node, explorer |
| [blockzero-wallet](https://github.com/Rexemre/blockzero-wallet) | Wallet guides & downloads |
| [blockzero-bridge](https://github.com/Rexemre/blockzero-bridge) | wBLOZ bridge (BSC) |

---

## Status

- **Mainnet — live.** Launched 2026-06-06 06:06:06 UTC. Public seed at `217.160.46.61:8210`, block explorer at [explorer.bloz.org](https://explorer.bloz.org).
- **Official pool — live.** [pool.bloz.org](https://pool.bloz.org) · PPLNS · RandomX CPU mining.
- Upstream baseline: Bitcoin Core v31.0 — see [UPSTREAM.md](UPSTREAM.md).

---

## Disclaimer

Block Zero is free, open-source software. BLOZ and TBLOZ are experimental coins
that carry **no** promised value, liquidity or return. Nothing here is financial
advice. You run the software and participate entirely at your own risk.

> **Warning:** Copycat sites (e.g. `.cc` domains) and third-party pools are **not affiliated** with Block Zero — we have no insight into their code and accept **no liability** for malware, wrong-chain mining, fraud, or unfair pool payouts. [Read the full warning →](https://github.com/Rexemre/blockzero-docs/blob/main/official-links.md#warning-copycat-sites--unofficial-services)
