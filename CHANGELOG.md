# Block Zero Changelog

## v1.0.0-rc39 (2026-10-29)

- **One-click address verification for bloz.org.** On bloz.org/community
  (Profile > Connections > "Open Block Zero wallet") the website now opens the
  wallet through a `bloz:verify?...` link. The wallet shows which site asks,
  the exact message, lets you pick (or create) a legacy address, signs with
  `signmessage` and sends only address + signature back over HTTPS. Links are
  accepted from bloz.org and its subdomains only; they carry a one-time token
  and expire after 15 minutes. No key ever leaves the wallet, no coins move.
  The link can also be pasted into File > Open URI… (e.g. when the wallet runs
  on another computer, scan the QR code on the website). The manual
  copy-and-paste way on the website still works.
- **Linux GUI wallet.** The release now ships `blockzero-<tag>-linux-<arch>.AppImage`
  (Qt bundled, one file, menu entry + icon + `bloz:` handler) in addition to the
  tarball. The tarball contains `share/block-zero.desktop`, icons and
  `install-desktop-entry.sh` to register the wallet with your desktop.
- **Crisp icons everywhere.** The Windows `.ico` had a single 16 px image, so
  the taskbar, Alt-Tab, shortcuts and the installer showed a blurry upscale.
  All icons are now generated from the official Block Zero mark
  (`contrib/brand/make-icons.py`): `.ico` with 16-256 px, 1024 px app icon,
  macOS `.icns`, Linux hicolor set, and the in-app window/tray icon is scaled
  smoothly with every common size. Testnet/signet icons use the same hue shift
  as the GUI.
- **Check for updates.** New menu item Help > Check for updates… and an
  automatic check once a day after start (Options > Main > "Check for wallet
  updates at startup", on by default, can be turned off). The check asks the
  GitHub Releases API for the latest version. On Windows "Download and install"
  fetches the one-click installer, verifies its SHA-256 against the published
  checksum, starts it and closes the wallet; elsewhere the release page opens.
  The release tag is compiled in (`-DBLOZ_RELEASE_TAG`), so Help > About and
  the update check know the real version.
- **Windows installer** registers the `bloz:` URL scheme (per user, removed on
  uninstall) and offers to close a running wallet instead of failing.

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
