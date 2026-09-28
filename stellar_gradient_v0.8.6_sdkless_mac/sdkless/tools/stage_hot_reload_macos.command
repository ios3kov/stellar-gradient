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
RUST_TOOLCHAIN="1.98.1"

rm -rf "$HOST_BUILD"
cp -R "$HOST_SRC" "$HOST_BUILD"

unset AESDK_ROOT AE_SDK_PATH PRSDK_ROOT || true
export RUSTFLAGS="--cfg threaded_rendering --cfg smart_render --cfg gpu_render --cfg catch_panics"
export MACOSX_DEPLOYMENT_TARGET="11.0"
export AE_HOT_LOADER_IMPL_LABEL="$LABEL"

echo "Verifying Stellar Gradient host/state contract..."
python3 "$ROOT/sdkless/tools/verify_shell_metadata.py" \
  "$ROOT/sdkless/build.rs" \
  "$ROOT/sdkless/shell/StellarGradientShell.cpp"
python3 "$ROOT/sdkless/tools/verify_hot_reload_state.py" \
  "$ROOT/sdkless/src/lib.rs" \
  "$ROOT/sdkless/shell/StellarGradientShell.cpp"

USE_RUSTUP=0
if command -v rustup >/dev/null 2>&1; then
  rustup toolchain install "$RUST_TOOLCHAIN" --profile minimal >/dev/null
  USE_RUSTUP=1
else
  active_rust="$(rustc --version 2>/dev/null | awk '{print $2}' || true)"
  [[ "$active_rust" == "$RUST_TOOLCHAIN" ]] || {
    echo "ERROR: Rust $RUST_TOOLCHAIN required for hot-reload Runtime ABI compatibility."
    exit 4
  }
  command -v cargo >/dev/null 2>&1 || {
    echo "ERROR: cargo is required."
    exit 4
  }
fi

echo "Building Stellar Gradient implementation: $LABEL"
if (( USE_RUSTUP )); then
  CARGO_TARGET_DIR="$TARGET" cargo +"$RUST_TOOLCHAIN" build \
    --release \
    --locked \
    --target "$TRIPLE" \
    --manifest-path "$HOST_BUILD/Cargo.toml"
else
  CARGO_TARGET_DIR="$TARGET" cargo build \
    --release \
    --locked \
    --target "$TRIPLE" \
    --manifest-path "$HOST_BUILD/Cargo.toml"
fi

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
