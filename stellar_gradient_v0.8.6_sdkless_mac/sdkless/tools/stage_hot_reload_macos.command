#!/bin/zsh
set -euo pipefail

ROOT="${0:A:h:h}"
TARGET="$ROOT/.hot-reload-target"
TRIPLE="aarch64-apple-darwin"
DEST_DIR="$HOME/Library/Application Support/AE Hot Loader/implementations/stellar-gradient"
DEST="$DEST_DIR/current.dylib"

export PATH="$HOME/.cargo/bin:$PATH"
unset AESDK_ROOT AE_SDK_PATH PRSDK_ROOT || true
export RUSTFLAGS="--cfg threaded_rendering --cfg smart_render --cfg gpu_render --cfg catch_panics"

echo "[1/3] Build Stellar Gradient implementation"
cd "$ROOT/sdkless"
CARGO_TARGET_DIR="$TARGET" cargo build --release --target "$TRIPLE"

SOURCE="$TARGET/$TRIPLE/release/libstellar_gradient_host.dylib"
[[ -f "$SOURCE" ]] || { echo "ERROR: missing $SOURCE"; exit 2; }

echo "[2/3] Stage signed implementation"
mkdir -p "$DEST_DIR"
TMP="$DEST_DIR/current.tmp.dylib"
rm -f "$TMP"
cp "$SOURCE" "$TMP"
codesign --force --sign - "$TMP"
mv -f "$TMP" "$DEST"
xattr -d com.apple.quarantine "$DEST" 2>/dev/null || true

echo "[3/3] Ready"
echo "  $DEST"
echo
echo "Keep After Effects open and click Reload Plugins in Window → AE Hot Loader."
