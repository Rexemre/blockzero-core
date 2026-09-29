# Block Zero Changelog

## v1.0.0-rc36 (2026-09-29)

- **Wallet: fallback fee enabled by default.** `DEFAULT_FALLBACK_FEE` is now
  1000 sat/kvB (0.00001 BLOZ/kvB), the same value as the minimum transaction
  fee the wallet already enforces. A freshly started wallet (no or stale
  `fee_estimates.dat`, e.g. after being closed for more than ~2.5 days) can
  now send right away instead of failing with "Fee estimation failed.
  Fallbackfee is disabled." `-fallbackfee` still overrides the default;
  `-fallbackfee=0` restores the old behaviour.

## v1.0.0-rc35 (2026-06-26)

- One-click Windows installer (`setup.exe`), plus a version-less
  `blockzero-windows-x64-setup.exe` alias for a stable download URL.
