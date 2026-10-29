#!/bin/sh
# Register the Block Zero wallet from this extracted release folder with your
# desktop: application menu entry, icon and the bloz: link handler used by
# bloz.org (one-click address verification, payment links).
#
#   ./install-desktop-entry.sh            # per user (~/.local), no root needed
#   ./install-desktop-entry.sh --remove   # undo
#
# The AppImage does not need this: AppImageLauncher (or a manual
# "Integrate" step) registers it the same way.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/bin/block-zero"
APPS="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICONS="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor"

if [ "$1" = "--remove" ]; then
  rm -f "$APPS/block-zero.desktop"
  for s in 16 22 24 32 48 64 128 256 512; do rm -f "$ICONS/${s}x${s}/apps/block-zero.png"; done
  command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APPS" || true
  echo "Block Zero desktop entry removed."
  exit 0
fi

[ -x "$BIN" ] || { echo "bin/block-zero not found next to this script. Extract the whole archive first." >&2; exit 1; }
mkdir -p "$APPS"
sed "s|^Exec=.*|Exec=\"$BIN\" %u|" "$HERE/share/block-zero.desktop" > "$APPS/block-zero.desktop"
for s in 16 22 24 32 48 64 128 256 512; do
  mkdir -p "$ICONS/${s}x${s}/apps"
  cp "$HERE/share/icons/block-zero-${s}.png" "$ICONS/${s}x${s}/apps/block-zero.png"
done
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APPS" || true
command -v xdg-mime >/dev/null 2>&1 && xdg-mime default block-zero.desktop x-scheme-handler/bloz || true
echo "Block Zero is now in your application menu and handles bloz: links."
