#!/bin/zsh
set -euo pipefail

HERE="${0:A:h}"
SOURCE="$HERE/StellarGradientImpl-candidate.dylib"
DEST_DIR="$HOME/Library/Application Support/AE Hot Loader/implementations/stellar-gradient"
DEST="$DEST_DIR/current.dylib"

[[ -f "$SOURCE" ]] || { echo "ERROR: missing $SOURCE"; exit 2; }

if ! pgrep -x "After Effects" >/dev/null 2>&1; then
  echo "ERROR: After Effects must already be running."
  exit 3
fi

codesign --verify --strict "$SOURCE"
mkdir -p "$DEST_DIR"
TMP="$DEST_DIR/current.tmp.dylib"
rm -f "$TMP"
cp "$SOURCE" "$TMP"
xattr -d com.apple.quarantine "$TMP" 2>/dev/null || true
mv -f "$TMP" "$DEST"

echo "Staged Stellar Gradient candidate:"
echo "  $DEST"
echo "Now click Window → AE Hot Loader → Reload Plugins."
