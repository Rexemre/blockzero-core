#!/usr/bin/env python3
"""Generate every wallet icon from the official Block Zero mark (contrib/brand/bloz-mark-512.png,
the same round badge bloz.org uses as its site icon).

Outputs (all checked in, so builds need no Pillow):
  src/qt/res/icons/bitcoin.png            1024x1024 app icon (Qt resource :/icons/bitcoin, macOS .icns source)
  src/qt/res/icons/bitcoin.ico            Windows icon, 16..256 px (exe resource, taskbar, shortcuts, installer)
  src/qt/res/icons/bitcoin_testnet.ico    hue-shifted like NetworkStyle (testnet green)
  src/qt/res/icons/bitcoin_signet.ico     hue-shifted like NetworkStyle (signet)
  src/qt/res/icons/bitcoin.icns, bloz.icns  macOS icon sets
  contrib/linux/icons/block-zero-<n>.png  hicolor set for .desktop / AppImage (16..512)

Usage: python contrib/brand/make-icons.py   (needs Pillow)
"""
from __future__ import annotations

import colorsys
import os
from pathlib import Path

from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "contrib" / "brand" / "bloz-mark-512.png"
ICONS = ROOT / "src" / "qt" / "res" / "icons"
LINUX = ROOT / "contrib" / "linux" / "icons"

ICO_SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
LINUX_SIZES = [16, 22, 24, 32, 48, 64, 128, 256, 512]


def scaled(img: Image.Image, size: int) -> Image.Image:
    """High-quality downscale; small sizes get a touch of sharpening so the rim and the 0 stay readable."""
    out = img.resize((size, size), Image.LANCZOS)
    if size <= 48:
        out = out.filter(ImageFilter.UnsharpMask(radius=1, percent=60, threshold=2))
    return out


def hue_shift(img: Image.Image, hue_deg: int, sat_minus: int) -> Image.Image:
    """Same transformation NetworkStyle applies at runtime (HSL hue rotation, saturation reduction)."""
    px = img.convert("RGBA").load()
    w, h = img.size
    out = Image.new("RGBA", (w, h))
    op = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            hh, ll, ss = colorsys.rgb_to_hls(r / 255, g / 255, b / 255)
            hh = (hh + hue_deg / 360) % 1.0
            s255 = ss * 255
            if s255 > sat_minus:
                s255 -= sat_minus
            r2, g2, b2 = colorsys.hls_to_rgb(hh, ll, s255 / 255)
            op[x, y] = (round(r2 * 255), round(g2 * 255), round(b2 * 255), a)
    return out


def save_ico(img: Image.Image, path: Path) -> None:
    """Write the ICO by hand: one PNG-compressed entry per size (Windows accepts PNG entries since Vista).
    Pillow's ICO writer derives all entries from a single base image, which is exactly the blur we want to avoid."""
    import io
    import struct

    blobs = []
    for s in ICO_SIZES:
        buf = io.BytesIO()
        scaled(img, s).save(buf, format="PNG", optimize=True)
        blobs.append((s, buf.getvalue()))
    header = struct.pack("<HHH", 0, 1, len(blobs))
    offset = len(header) + 16 * len(blobs)
    entries = b""
    for s, blob in blobs:
        entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    path.write_bytes(header + entries + b"".join(b for _, b in blobs))


def save_icns(img: Image.Image, path: Path) -> None:
    big = img.resize((1024, 1024), Image.LANCZOS)
    big.save(path, format="ICNS")


def main() -> None:
    src = Image.open(SRC).convert("RGBA")
    assert src.size == (512, 512), src.size
    ICONS.mkdir(parents=True, exist_ok=True)
    LINUX.mkdir(parents=True, exist_ok=True)

    src.resize((1024, 1024), Image.LANCZOS).save(ICONS / "bitcoin.png", optimize=True)
    save_ico(src, ICONS / "bitcoin.ico")
    save_ico(hue_shift(src, 70, 30), ICONS / "bitcoin_testnet.ico")
    save_ico(hue_shift(src, 35, 15), ICONS / "bitcoin_signet.ico")
    save_icns(src, ICONS / "bitcoin.icns")
    save_icns(src, ICONS / "bloz.icns")
    for s in LINUX_SIZES:
        scaled(src, s).save(LINUX / f"block-zero-{s}.png", optimize=True)
    print("icons written to", os.path.relpath(ICONS, ROOT), "and", os.path.relpath(LINUX, ROOT))


if __name__ == "__main__":
    main()
