#!/bin/zsh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
HOST_SRC="$ROOT/sdkless"
HOST_BUILD="$ROOT/.sdkless-hot-reload"
TARGET="$ROOT/.sdkless-hot-target"
TRIPLE="aarch64-apple-darwin"
SOURCE="$TARGET/$TRIPLE/release/libstellar_gradient_host.dylib"
DEST_DIR="$HOME/Library/Application Support/AE Hot Loader/implementations/stellar-gradient"
DEST="$DEST_DIR/current.dylib"
LABEL="${1:-stellar-gradient-dev}"

rm -rf "$HOST_BUILD"
cp -R "$HOST_SRC" "$HOST_BUILD"

unset AESDK_ROOT AE_SDK_PATH PRSDK_ROOT || true
export RUSTFLAGS="--cfg threaded_rendering --cfg smart_render --cfg gpu_render --cfg catch_panics"
export MACOSX_DEPLOYMENT_TARGET="11.0"
export AE_HOT_LOADER_IMPL_LABEL="$LABEL"

echo "Building Stellar Gradient implementation: $LABEL"
CARGO_TARGET_DIR="$TARGET" cargo build \
  --release \
  --target "$TRIPLE" \
  --manifest-path "$HOST_BUILD/Cargo.toml"

[[ -f "$SOURCE" ]] || { echo "ERROR: missing implementation: $SOURCE"; exit 2; }

mkdir -p "$DEST_DIR"
TMP="$DEST_DIR/current.tmp.dylib"
rm -f "$TMP"
cp "$SOURCE" "$TMP"
codesign --force --sign - "$TMP"
xattr -d com.apple.quarantine "$TMP" 2>/dev/null || true
mv -f "$TMP" "$DEST"

echo
echo "Staged Stellar Gradient implementation:"
echo "  $DEST"
echo "  label=$LABEL"
echo
echo "Keep After Effects open and click Window → AE Hot Loader → Reload Plugins."
