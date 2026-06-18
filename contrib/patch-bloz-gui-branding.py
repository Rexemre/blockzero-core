#!/usr/bin/env python3
"""Patch user-visible Bitcoin branding to Block Zero / BLOZ in GUI sources."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Order matters: longer phrases first.
REPLACEMENTS = [
    ("Partially Signed Bitcoin Transaction", "Partially Signed BLOZ Transaction (PSBT)"),
    ("Partially Signed BLOZ Transaction (PSBT) (PSBT)", "Partially Signed BLOZ Transaction (PSBT)"),
    ("Bitcoin block chain", "Block Zero blockchain"),
    ("Bitcoin blockchain", "Block Zero blockchain"),
    ("Bitcoin network", "Block Zero network"),
    ("bitcoin network", "Block Zero network"),
    ("Bitcoin node", "Block Zero node"),
    ("Bitcoin client", "Block Zero client"),
    ("Bitcoin command-line", "Block Zero command-line"),
    ("Bitcoin addresses", "BLOZ addresses"),
    ("Bitcoin address", "BLOZ address"),
    ("Invalid Bitcoin address", "Invalid BLOZ address"),
    ("valid Bitcoin address", "valid BLOZ address"),
    ("bitcoin: URIs", "bloz: URIs"),
    ("bitcoin: URI", "bloz: URI"),
    ("bitcoin: click-to-pay", "bloz: click-to-pay"),
    ("'bitcoin://'", "'bloz://'"),
    ("Use 'bitcoin:' instead", "Use 'bloz:' instead"),
    ("Open a bitcoin: URI", "Open a bloz: URI"),
    ("Open bitcoin URI", "Open bloz URI"),
    ("spend bitcoins", "spend BLOZ"),
    ("bitcoins that", "BLOZ that"),
    ("Bitcoin Transaction", "BLOZ transaction"),
    (">Bitcoin<", ">Block Zero<"),
    ('tr("Bitcoin")', 'tr("Block Zero")'),
    ("The Bitcoin Core developers", "The Block Zero developers"),
    ("Bitcoin Core", "Block Zero"),
]

UI_FILES = list((ROOT / "src/qt/forms").glob("*.ui"))
CPP_FILES = list((ROOT / "src/qt").glob("*.cpp"))
LOCALE_FILES = list((ROOT / "src/qt/locale").glob("bitcoin_de.ts"))


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


def patch_cmake() -> None:
    path = ROOT / "CMakeLists.txt"
    text = path.read_text(encoding="utf-8")
    text = text.replace('set(CLIENT_NAME "Bitcoin Core")', 'set(CLIENT_NAME "Block Zero")')
    text = text.replace('DESCRIPTION "Bitcoin client software"', 'DESCRIPTION "Block Zero client software"')
    text = text.replace('HOMEPAGE_URL "https://bitcoincore.org/"', 'HOMEPAGE_URL "https://bloz.org/"')
    text = text.replace(
        'set(CLIENT_BUGREPORT "https://github.com/bitcoin/bitcoin/issues")',
        'set(CLIENT_BUGREPORT "https://github.com/Rexemre/blockzero-core/issues")',
    )
    text = text.replace('project(BitcoinCore', 'project(BlockZeroCore')
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_clientversion() -> None:
    path = ROOT / "src/clientversion.cpp"
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        """    // Make sure Bitcoin Core copyright is not removed by accident
    if (copyright_devs.find("Bitcoin Core") == std::string::npos) {
        strCopyrightHolders += "\\n" + strPrefix + "The Bitcoin Core developers";
    }""",
        """    // Block Zero: upstream attribution lives in COPYING / README, not the splash screen.""",
    )
    text = text.replace(
        'const std::string URL_SOURCE_CODE = "<https://github.com/bitcoin/bitcoin>";',
        'const std::string URL_SOURCE_CODE = "<https://github.com/Rexemre/blockzero-core>";',
    )
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_splash() -> None:
    path = ROOT / "src/qt/splashscreen.cpp"
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        'QString copyrightText   = QString::fromUtf8(CopyrightHolders(strprintf("\\xc2\\xa9 %u-%u ", 2009, COPYRIGHT_YEAR)).c_str());',
        'QString copyrightText   = QString::fromUtf8(CopyrightHolders(strprintf("\\xc2\\xa9 %u-%u ", 2026, COPYRIGHT_YEAR)).c_str());',
    )
    text = text.replace("// draw the bitcoin icon", "// draw the app icon")
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_utilitydialog() -> None:
    path = ROOT / "src/qt/utilitydialog.cpp"
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        'QString header = "The bitcoin-qt application provides a graphical interface for interacting with " CLIENT_NAME ".\\n\\n"',
        'QString header = "The Block Zero wallet provides a graphical interface for interacting with " CLIENT_NAME ".\\n\\n"',
    )
    text = text.replace("bitcoind with a user-friendly", "the Block Zero node with a user-friendly")
    text = text.replace("Usage: bitcoin-qt [options] [URI]", "Usage: bitcoin-qt [options] [URI]  # binary name unchanged")
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_qt_res() -> None:
    path = ROOT / "src/qt/res/bitcoin-qt-res.rc"
    text = path.read_text(encoding="utf-8")
    text = text.replace('CLIENT_NAME " (GUI node for Bitcoin)"', 'CLIENT_NAME " (Block Zero wallet)"')
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_macos_plist() -> None:
    path = ROOT / "share/qt/Info.plist.in"
    text = path.read_text(encoding="utf-8")
    replacements = [
        ("<string>x86_64</string>", "<string>arm64</string>"),
        ("<string>bitcoin.icns</string>", "<string>bloz.icns</string>"),
        ("<string>Bitcoin-Qt</string>", "<string>Block Zero</string>"),
        ("org.bitcoinfoundation.Bitcoin-Qt", "org.blockzero.BlockZero"),
        ("org.bitcoin.BitcoinPayment", "org.blockzero.BlozPayment"),
        ("<string>bitcoin</string>", "<string>bloz</string>"),
    ]
    for old, new in replacements:
        text = text.replace(old, new)
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_macos_deploy() -> None:
    path = ROOT / "cmake/module/Maintenance.cmake"
    text = path.read_text(encoding="utf-8")
    text = text.replace('set(macos_app "Bitcoin-Qt.app")', 'set(macos_app "Block Zero.app")')
    text = text.replace("res/icons/bitcoin.icns", "res/icons/bloz.icns")
    text = text.replace("Resources/bitcoin.icns", "Resources/bloz.icns")
    text = text.replace("MacOS/Bitcoin-Qt", 'MacOS/Block Zero')
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_german_locale() -> None:
    de_rules = [
        ("Bitcoin Core", "Block Zero"),
        ("Bitcoin-Netzwerk", "Block-Zero-Netzwerk"),
        ("Bitcoin-Blockchain", "Block-Zero-Blockchain"),
        ("Bitcoin-Adressen", "BLOZ-Adressen"),
        ("Bitcoin-Adresse", "BLOZ-Adresse"),
        ("Bitcoins", "BLOZ"),
        ("Bitcoin-Client", "Block-Zero-Client"),
        ("Bitcoin-Transaktion", "BLOZ-Transaktion"),
        ("teilsignierte Bitcoin-Transaktion", "teilsignierte BLOZ-Transaktion (PSBT)"),
    ]
    for path in LOCALE_FILES:
        text = path.read_text(encoding="utf-8")
        for old, new in de_rules:
            text = text.replace(old, new)
        path.write_text(text, encoding="utf-8", newline="\n")


def main() -> None:
    patch_cmake()
    patch_clientversion()
    patch_splash()
    patch_utilitydialog()
    patch_qt_res()
    patch_macos_plist()
    patch_macos_deploy()
    changed = 0
    for path in UI_FILES + CPP_FILES + LOCALE_FILES:
        if patch_file(path):
            changed += 1
            print(f"patched {path.relative_to(ROOT)}")
    patch_german_locale()
    print(f"done ({changed} qt files updated)")


if __name__ == "__main__":
    main()
