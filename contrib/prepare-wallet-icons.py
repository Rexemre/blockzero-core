#!/usr/bin/env python3
"""Generate Block Zero wallet icons from site brand assets."""
from __future__ import annotations

from pathlib import Path

try:
    from PIL import Image
except ImportError:
    raise SystemExit("pip install Pillow")

ROOT = Path(__file__).resolve().parents[1]
ICONS = ROOT / "src/qt/res/icons"
PIXMAPS = ROOT / "share/pixmaps"
DOWNLOADS = Path(r"C:\Users\Marlon\Downloads")
SITE_ASSETS = Path(r"C:\Users\Marlon\MarlonMoralesServer\sites\blockzero\assets")

APP_ICON = DOWNLOADS / "favicon-15-06V2.png"
HERO_LOGO = SITE_ASSETS / "bloz-logo.png"


def resize_square(img: Image.Image, size: int) -> Image.Image:
    return img.convert("RGBA").resize((size, size), Image.LANCZOS)


def make_app_icon_png() -> None:
    src = Image.open(APP_ICON)
    out = ICONS / "bitcoin.png"
    resize_square(src, 1024).save(out, optimize=True)
    print(f"app icon png: {out}")


def make_app_icon_icns() -> None:
    # macOS app bundle icon. The release CI regenerates this with native
    # sips/iconutil; this provides a committed fallback for the cmake deploy path.
    src = Image.open(APP_ICON)
    out = ICONS / "bloz.icns"
    try:
        resize_square(src, 1024).save(out, format="ICNS")
        print(f"app icon icns: {out}")
    except (OSError, ValueError) as exc:
        print(f"skip icns (Pillow cannot write ICNS here: {exc}); CI builds it via iconutil")


def make_app_icon_ico() -> None:
    src = Image.open(APP_ICON)
    sizes = [16, 32, 48, 256]
    images = [resize_square(src, s) for s in sizes]
    for path in (ICONS / "bitcoin.ico", PIXMAPS / "bitcoin.ico"):
        path.parent.mkdir(parents=True, exist_ok=True)
        images[0].save(
            path,
            format="ICO",
            sizes=[(s, s) for s in sizes],
            append_images=images[1:],
        )
        print(f"app icon ico: {path}")


def make_splash_logo() -> None:
    src = Image.open(HERO_LOGO).convert("RGBA")
    out = ICONS / "bloz-splash.png"
    max_w, max_h = 1200, 807
    if src.width > max_w or src.height > max_h:
        scale = min(max_w / src.width, max_h / src.height)
        src = src.resize((max(1, int(src.width * scale)), max(1, int(src.height * scale))), Image.LANCZOS)
    src.save(out, optimize=True)
    print(f"splash logo: {out} ({src.width}x{src.height})")


def main() -> None:
    for path in (APP_ICON, HERO_LOGO):
        if not path.exists():
            raise SystemExit(f"missing brand asset: {path}")
    make_app_icon_png()
    make_app_icon_ico()
    make_app_icon_icns()
    make_splash_logo()
    print("done")


if __name__ == "__main__":
    main()
