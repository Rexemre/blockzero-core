#!/usr/bin/env python3
"""Patch user-visible Bitcoin/BTC/satoshi wording to Block Zero/BLOZ/rex in
RPC help text, RPC error messages, CLI --help/--version output, and
init/log messages that users actually see.

Deliberately NOT touched (see report for the full list): MIT copyright/license
headers, BIP references and bips.mediawiki links, protocol/network constants
(UA_NAME "Satoshi", the P2P message-signing magic, DNS seed hostnames, the
Tor/I2P address-derivation comment), binary/executable file names
(bitcoind, bitcoin-cli, bitcoin-qt, ...), the bitcoin.conf/settings.json
filenames, and RPC JSON field/key names (only their help-text descriptions
are reworded).

Order matters: longer/more specific phrases first.
"""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Phrase-level replacements applied across the RPC help/error text files below.
REPLACEMENTS = [
    ("1BTC/kvB", "1BLOZ/kvB"),
    ("partially signed Bitcoin transaction", "partially signed BLOZ transaction"),
    ("Invalid Bitcoin address", "Invalid BLOZ address"),
    ("newly generated bitcoin", "newly generated BLOZ"),
    ("Bitcoin address", "BLOZ address"),
    ("bitcoin address", "BLOZ address"),
    (r"sending bitcoin\n", r"sending BLOZ\n"),
    ("less bitcoins than you enter", "less BLOZ than you enter"),
    (r" BTC\n", r" BLOZ\n"),
    (" BTC ", " BLOZ "),
    ("satoshis", "rex"),
    ("satoshi", "rex"),
]

RPC_FILES = sorted((ROOT / "src/rpc").glob("*.cpp")) + sorted((ROOT / "src/wallet/rpc").glob("*.cpp"))

# One-off phrases in files outside src/rpc and src/wallet/rpc, kept separate
# so a stray match there can't silently slip into the generic RPC pass above.
EXTRA_REPLACEMENTS: list[tuple[Path, str, str]] = [
    (ROOT / "src/wallet/rpc/spend.cpp", "sat/vB", "rex/vB"),
    (ROOT / "src/rpc/net.cpp", "The bitcoin server IP and port", "The Block Zero server IP and port"),
    (ROOT / "src/bitcoin-util.cpp", "bitcoin related functionality", "Block Zero-related functionality"),
    (ROOT / "src/bitcoin-tx.cpp", "modifying bitcoin transactions", "modifying BLOZ transactions"),
    (ROOT / "src/bitcoin-tx.cpp", "hex-encoded bitcoin transaction", "hex-encoded BLOZ transaction"),
    (ROOT / "src/bitcoind.cpp", "connects to the Bitcoin network", "connects to the Block Zero network"),
    (ROOT / "src/bitcoind.cpp", "backbone of the Bitcoin network", "backbone of the Block Zero network"),
    (ROOT / "src/init.cpp", "if bitcoin is started", "if Block Zero is started"),
    (ROOT / "src/mapport.cpp", "instance of bitcoin running", "instance of Block Zero running"),
    (ROOT / "src/util/exception.cpp", 'pszModule = "bitcoin";', 'pszModule = "Block Zero";'),
    (ROOT / "src/wallet/wallettool.cpp", "safety of your Bitcoin,", "safety of your BLOZ,"),
    # Smallest-unit rename (Discord decision, 2026-09-29: long form "Rexemre",
    # short form "rex"), applied to the RPC-facing currency-atom constants so
    # fee_rate help text and examples say "rex/vB" instead of "sat"/"szat".
    (ROOT / "src/policy/feerate.h",
     'const std::string CURRENCY_ATOM = "szat"; // One indivisible minimum value unit (1 BLOZ = 100,000,000 szat)',
     'const std::string CURRENCY_ATOM = "rex"; // One indivisible minimum value unit (1 BLOZ = 100,000,000 rex)'),
    (ROOT / "src/kernel/chainparams.h", 'std::string m_currency_unit{"BTC"};', 'std::string m_currency_unit{"BLOZ"};'),
    (ROOT / "src/kernel/chainparams.h", 'std::string m_currency_atom{"sat"};', 'std::string m_currency_atom{"rex"};'),
    (ROOT / "src/kernel/chainparams.cpp", 'm_currency_atom = "sat";', 'm_currency_atom = "rex";'),
    (ROOT / "src/kernel/chainparams.cpp", 'm_currency_atom = "tsat";', 'm_currency_atom = "trex";'),
]


def patch_text(text: str) -> str:
    for old, new in REPLACEMENTS:
        text = text.replace(old, new)
    return text


def patch_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = patch_text(original)
    if updated != original:
        path.write_text(updated, encoding="utf-8", newline="\n")
        return True
    return False


def patch_extra() -> int:
    count = 0
    for path, old, new in EXTRA_REPLACEMENTS:
        original = path.read_text(encoding="utf-8")
        updated = original.replace(old, new)
        if updated != original:
            path.write_text(updated, encoding="utf-8", newline="\n")
            count += 1
    return count


def main() -> None:
    changed = [str(p.relative_to(ROOT)) for p in RPC_FILES if patch_file(p)]
    extra_changed = patch_extra()
    print(f"Patched {len(changed)} RPC source file(s):")
    for c in changed:
        print(f"  {c}")
    print(f"Patched {extra_changed} extra replacement(s) in one-off files.")


if __name__ == "__main__":
    main()
