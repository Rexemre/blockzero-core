# BLOCK ZERO

### Modern Bitcoin code. A second chance at Genesis.

**CPU-mineable · Fair launch · Pure proof-of-work · No presale · No insiders · No premine**

> Block Zero rebuilds the moment Bitcoin began — when anyone could open a laptop,
> point it at the network, and mine real blocks. Same battle-tested Bitcoin Core
> engine, but with a proof-of-work that keeps ASIC and GPU farms out, so the
> people mining are people, not warehouses.

---

## Official links

| | |
|---|---|
| **Website** | https://bloz.org |
| **Pool** | https://pool.bloz.org *(test release)* |
| **Explorer** | https://explorer.bloz.org · testnet: https://texplorer.bloz.org |
| **Bridge** | https://bridge.bloz.org |
| **Discord** | https://discord.gg/FbJzrwAU2W |
| **X (Twitter)** | https://x.com/BlockZeroBLOZ |
| **Full list** | [official-links.md](https://github.com/Rexemre/blockzero-docs/blob/main/official-links.md) |

**Mainnet seed:** `217.160.46.61:8210`

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
- **It feels like 2009.** Download the node, start mining, earn blocks. The way Bitcoin worked before industrial mining took over.

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
cd blockzero-ops\scripts\testnet
.\install-windows.ps1
cd ..\mainnet
.\mine-mainnet.ps1 -Status   # sync to the public seed first
.\mine-mainnet.ps1           # mine
```

**Public seed:** `217.160.46.61:8210` · **Block explorer:** https://explorer.bloz.org

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
| [blockzero-wallet](https://github.com/Rexemre/blockzero-wallet) | Wallet (in development) |

---

## Status

- **Mainnet — live.** Launched 2026-06-06 06:06:06 UTC. Public seed at `217.160.46.61:8210`, block explorer at [explorer.bloz.org](https://explorer.bloz.org).
- **Testnet — live.** Public seed at `217.160.46.61:18210`, explorer at [texplorer.bloz.org](https://texplorer.bloz.org).
- Upstream baseline: Bitcoin Core v31.0 — see [UPSTREAM.md](UPSTREAM.md).

---

## Disclaimer

Block Zero is free, open-source software. BLOZ and TBLOZ are experimental coins
that carry **no** promised value, liquidity or return. Nothing here is financial
advice. You run the software and participate entirely at your own risk.
