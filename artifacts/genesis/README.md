# Block Zero genesis artifacts

Published, reproducible genesis parameters for each network.

| File | Network | Status |
|------|---------|--------|
| `mainnet.json` | Mainnet (2026-06-06) | Mined · **live** |
| `testnet.json` | Testnet (2026-06-04) | Mined · live |

## Mainnet genesis (2026-06-06)

**Genesis message (coinbase scriptSig), Satoshi-style:**

```text
The Times 06/Jun/2026 Block Zero - a second chance at Genesis
```

| Field | Value |
|-------|-------|
| `nTime` | `1780725966` (2026-06-06 06:06:06 UTC) |
| `nBits` | `0x1e3fffff` |
| `nVersion` | `1` |
| Reward | 50 BLOZ |

Full mined values (`nonce`, `hashGenesisBlock`, `hashMerkleRoot`, `powHash`) are in `mainnet.json`.

## Testnet genesis (2026-06-04)

**Genesis message (coinbase scriptSig), Satoshi-style:**

```text
The Times 04/Jun/2026 Block Zero - a second chance at Genesis
```

| Field | Value |
|-------|-------|
| `nTime` | `1780531200` (2026-06-04 00:00:00 UTC) |
| `nBits` | `0x1e3fffff` |
| `nVersion` | `1` |
| Reward | 50 TBLOZ |

Full mined values (`nonce`, `hashGenesisBlock`, `hashMerkleRoot`, `powHash`) are in `testnet.json`.

## Reproduce

```powershell
cd blockzero-core
.\scripts\genesis\mine-mainnet-genesis.ps1
.\scripts\genesis\mine-testnet-genesis.ps1
```

Native Windows is fastest for RandomX, but any platform that builds `bz-genesis-miner` works.

## Verify

Anyone can re-run `bz-genesis-miner` and confirm the printed `hashGenesis` matches the published JSON.
