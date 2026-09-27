#!/bin/zsh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
HOST_SRC="$ROOT/sdkless"
HOST_BUILD="$ROOT/.sdkless-build"
REPORT="$ROOT/mac_first_build_report"
ZIP="$ROOT/mac_first_build_report.zip"
TARGET="$ROOT/.sdkless-target"
TRIPLE="aarch64-apple-darwin"
export MACOSX_DEPLOYMENT_TARGET="11.0"
BUNDLE="$ROOT/dist/mac/StellarGradient.plugin"
DEST="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"

rm -rf "$REPORT" "$ZIP"
mkdir -p "$REPORT"
LOG="$REPORT/build.log"
exec > >(tee -a "$LOG") 2>&1

finish_report() {
  set +e
  [[ -d "$BUNDLE" ]] && cp "$BUNDLE/Contents/Info.plist" "$REPORT/Info.plist" 2>/dev/null || true
  [[ -d "$BUNDLE" ]] && codesign -dv --verbose=4 "$BUNDLE" >"$REPORT/codesign.txt" 2>&1 || true
  if command -v ditto >/dev/null 2>&1; then ditto -c -k --sequesterRsrc --keepParent "$REPORT" "$ZIP" >/dev/null 2>&1 || true; fi
  echo ""
  echo "Diagnostic report: $ZIP"
}
trap finish_report EXIT

if [[ "$(uname -s)" != "Darwin" ]]; then echo "ERROR: run on macOS"; exit 2; fi
if [[ "$(uname -m)" != "arm64" ]]; then echo "ERROR: first test build currently targets Apple Silicon (arm64)"; exit 2; fi
command -v xcrun >/dev/null 2>&1 || { echo "ERROR: Xcode Command Line Tools missing"; exit 2; }
xcrun -f clang++ >/dev/null 2>&1 || { echo "ERROR: clang++ missing"; exit 2; }
command -v codesign >/dev/null 2>&1 || { echo "ERROR: codesign missing"; exit 2; }
command -v python3 >/dev/null 2>&1 || { echo "ERROR: python3 missing"; exit 2; }

rust_meets_minimum() {
  local version="$1"
  python3 - "$version" <<'PYVER'
import re, sys
m = re.match(r"^(\d+)\.(\d+)\.(\d+)", sys.argv[1])
if not m:
    raise SystemExit(1)
v = tuple(map(int, m.groups()))
raise SystemExit(0 if v >= (1, 85, 0) else 1)
PYVER
}

install_user_rust() {
  echo "[bootstrap] Installing/updating user-local stable Rust (Rust >=1.85 required for edition 2024)..."
  command -v curl >/dev/null 2>&1 || { echo "ERROR: curl missing"; exit 2; }
  if ! command -v rustup >/dev/null 2>&1; then
    RUSTUP_INIT_SKIP_PATH_CHECK=yes curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
      RUSTUP_INIT_SKIP_PATH_CHECK=yes sh -s -- -y --profile minimal --default-toolchain stable
  fi
  export PATH="$HOME/.cargo/bin:$PATH"
  [[ -f "$HOME/.cargo/env" ]] && source "$HOME/.cargo/env"
  rustup toolchain install stable --profile minimal >/dev/null
  rustup default stable >/dev/null
  rustup component add rustfmt clippy --toolchain stable >/dev/null
  rehash
}

NEED_USER_RUST=0
if ! command -v cargo >/dev/null 2>&1 || ! command -v rustc >/dev/null 2>&1; then
  NEED_USER_RUST=1
else
  RUST_VERSION="$(rustc --version | awk '{print $2}')"
  if ! rust_meets_minimum "$RUST_VERSION"; then
    echo "[bootstrap] Existing Rust $RUST_VERSION is too old; need >=1.85."
    NEED_USER_RUST=1
  elif ! cargo fmt --version >/dev/null 2>&1 || ! cargo clippy --version >/dev/null 2>&1; then
    echo "[bootstrap] Existing Rust is missing rustfmt and/or clippy."
    NEED_USER_RUST=1
  fi
fi

if (( NEED_USER_RUST )); then
  install_user_rust
fi

RUST_VERSION="$(rustc --version | awk '{print $2}')"
if ! rust_meets_minimum "$RUST_VERSION"; then
  echo "ERROR: Rust >=1.85 required, but active rustc is $RUST_VERSION"
  exit 2
fi
cargo fmt --version >/dev/null 2>&1 || { echo "ERROR: rustfmt unavailable"; exit 2; }
cargo clippy --version >/dev/null 2>&1 || { echo "ERROR: clippy unavailable"; exit 2; }

cargo --version
rustc --version

cd "$ROOT"
echo "[1/8] Frozen-core + SDK-less contracts"
python3 tools/verify_code_freeze.py
python3 tools/verify_metal_perf.py
python3 tools/verify_ae_contract.py
python3 tools/verify_sdkless_host.py
python3 tools/verify_v08_host_freeze.py

echo "[2/8] Native bridge parity"
TMP="$ROOT/.mac-preflight"
rm -rf "$TMP" && mkdir -p "$TMP"
xcrun clang++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off \
  sdkless/bridge/bridge_parity_test.cpp sdkless/bridge/StellarBridge.cpp src/core/RenderPlan.cpp src/cpu/ReferenceRenderer.cpp \
  -o "$TMP/bridge_parity"
"$TMP/bridge_parity"

echo "[3/8] Sanitizer smoke"
xcrun clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  sdkless/bridge/bridge_parity_test.cpp sdkless/bridge/StellarBridge.cpp src/core/RenderPlan.cpp src/cpu/ReferenceRenderer.cpp \
  -o "$TMP/bridge_asan"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$TMP/bridge_asan"

echo "[4/8] Rust host quality gates"
unset AESDK_ROOT AE_SDK_PATH PRSDK_ROOT || true
unset RUSTFLAGS CARGO_ENCODED_RUSTFLAGS || true
# Cargo build-script cfg emission proved insufficient on the real Mac with
# after-effects 0.4.0 macro expansion. Pin the destination-crate cfgs in the
# actual rustc invocation so MFR/SmartFX/GPU/panic behavior cannot drift.
export RUSTFLAGS="--cfg threaded_rendering --cfg smart_render --cfg gpu_render --cfg catch_panics"
# Keep the SHA-frozen source tree byte-for-byte unchanged. Format and compile an
# ephemeral copy whose parent is still the project root, so build.rs resolves
# ../src to the frozen C++/Metal core exactly as in the canonical host.
rm -rf "$HOST_BUILD" "$TARGET"
cp -R "$HOST_SRC" "$HOST_BUILD"
cd "$HOST_BUILD"
CARGO_TARGET_DIR="$TARGET" cargo fmt --all
CARGO_TARGET_DIR="$TARGET" cargo fmt --all -- --check
CARGO_TARGET_DIR="$TARGET" cargo check --release --target "$TRIPLE"
CARGO_TARGET_DIR="$TARGET" cargo test --release --target "$TRIPLE"
CARGO_TARGET_DIR="$TARGET" cargo clippy --release --target "$TRIPLE" -- -D warnings

echo "[5/8] SDK-less Rust host release build (no Adobe SDK)"
CARGO_TARGET_DIR="$TARGET" cargo build --release --target "$TRIPLE"

BIN="$TARGET/$TRIPLE/release/libstellar_gradient_host.dylib"
RSRC="$TARGET/$TRIPLE/release/stellar_gradient_host.rsrc"
PKG="$TARGET/$TRIPLE/release/stellar_gradient_host_PkgInfo"
PLIST="$TARGET/$TRIPLE/release/stellar_gradient_host_Info.plist"
for f in "$BIN" "$RSRC" "$PKG" "$PLIST"; do [[ -f "$f" ]] || { echo "ERROR: missing build output $f"; exit 3; }; done

echo "[6/8] Bundle stable shell + implementation + local signing"
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Frameworks" "$BUNDLE/Contents/Resources"

SHELL_BIN="$BUNDLE/Contents/MacOS/StellarGradient"
IMPL_BIN="$BUNDLE/Contents/Frameworks/libstellar_gradient_impl.dylib"

xcrun clang++ \
  -std=c++17 -O2 -arch arm64 -mmacosx-version-min=11.0 -dynamiclib -fvisibility=hidden \
  "$HOST_BUILD/shell/StellarGradientShell.cpp" \
  -o "$SHELL_BIN"

cp "$BIN" "$IMPL_BIN"
cp "$RSRC" "$BUNDLE/Contents/Resources/StellarGradient.rsrc"
cp "$PKG" "$BUNDLE/Contents/PkgInfo"
cp "$PLIST" "$BUNDLE/Contents/Info.plist"
/usr/libexec/PlistBuddy -c 'Set :CFBundleExecutable StellarGradient' "$BUNDLE/Contents/Info.plist" || /usr/libexec/PlistBuddy -c 'Add :CFBundleExecutable string StellarGradient' "$BUNDLE/Contents/Info.plist"
/usr/libexec/PlistBuddy -c 'Set :CFBundleIdentifier com.stellarlabs.StellarGradient' "$BUNDLE/Contents/Info.plist" || /usr/libexec/PlistBuddy -c 'Add :CFBundleIdentifier string com.stellarlabs.StellarGradient' "$BUNDLE/Contents/Info.plist"
/usr/libexec/PlistBuddy -c 'Set :CFBundleName "Stellar Gradient"' "$BUNDLE/Contents/Info.plist" || true
xattr -dr com.apple.quarantine "$BUNDLE" 2>/dev/null || true
codesign --force --sign - "$IMPL_BIN"
codesign --force --deep --options runtime --sign - "$BUNDLE"
codesign --verify --deep --strict "$BUNDLE"
file "$SHELL_BIN"
nm -gU "$SHELL_BIN" | grep -E 'EffectMain|PluginDataEntryFunction2|AEHotLoader_ShellReload'
nm -gU "$IMPL_BIN" | grep -E 'EffectMain|AEHotLoader_ImplementationABI|AEHotLoader_ImplementationStateABI|AEHotLoader_ImplementationKey|AEHotLoader_ImplementationLabel'
plutil -lint "$BUNDLE/Contents/Info.plist"

echo "[7/8] Install"
mkdir -p "$DEST"
rm -rf "$DEST/StellarGradient.plugin"
cp -R "$BUNDLE" "$DEST/StellarGradient.plugin"
codesign --verify --deep --strict "$DEST/StellarGradient.plugin"

echo "[8/8] Environment + launch AE"
{
  date
  sw_vers
  uname -a
  system_profiler SPHardwareDataType SPDisplaysDataType 2>/dev/null || true
  cargo --version
  rustc --version
  xcodebuild -version 2>/dev/null || xcrun clang++ --version
} | tee "$REPORT/environment.txt"

AE_APPS=(/Applications/Adobe\ After\ Effects*/Adobe\ After\ Effects*.app(N))
if (( ${#AE_APPS[@]} )); then
  open "$AE_APPS[1]"
else
  echo "After Effects app not found automatically; open it manually."
fi

echo ""
echo "BUILD/INSTALL PASS"
echo "Plugin: $DEST/StellarGradient.plugin"
echo "In AE: Effect > Stellar > Stellar Gradient"
echo "After AE opens, use docs/MAC_TEST.md and send back $ZIP if anything fails."
read "?Press Enter to close..."
