#!/bin/zsh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${AE_SDK_PATH:-${1:-}}"
if [[ -z "$SDK" || ! -f "$SDK/Examples/Headers/AE_Effect.h" ]]; then
  echo "Set AE_SDK_PATH to the Adobe After Effects SDK root (Examples/Headers/AE_Effect.h must exist)."
  exit 2
fi

python3 "$ROOT/tools/verify_metadata.py" >/dev/null
python3 "$ROOT/tools/verify_metal_perf.py" >/dev/null
python3 "$ROOT/tools/verify_ae_contract.py" >/dev/null
python3 "$ROOT/tools/verify_code_freeze.py" >/dev/null
python3 "$ROOT/tools/embed_metal.py" >/dev/null

ARCH="${ARCH:-$(uname -m)}"
MIN_MACOS="${MACOSX_DEPLOYMENT_TARGET:-12.0}"
OUT="$ROOT/build/mac/$ARCH"
BUNDLE="$OUT/StellarGradient.plugin"
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"
cp "$ROOT/Mac/StellarGradient.plugin-Info.plist" "$BUNDLE/Contents/Info.plist"

INCS=(
  -I"$SDK/Examples/Headers" -I"$SDK/Examples/Headers/SP" -I"$SDK/Examples/Util"
  -I"$SDK/Examples/Resources" -I"$SDK/Examples/GPUUtils" -I"$SDK/Examples/GPUUtils/PrGPU/KernelSupport"
  -I"$ROOT/src"
)

xcrun clang++ -std=c++17 -O3 -DNDEBUG -arch "$ARCH" -mmacosx-version-min="$MIN_MACOS" \
  -fno-fast-math -fno-unsafe-math-optimizations \
  -fPIC -fvisibility=hidden -fvisibility-inlines-hidden -bundle \
  "${INCS[@]}" \
  "$ROOT/src/plugin/StellarGradient.mm" \
  "$ROOT/src/core/RenderPlan.cpp" \
  "$ROOT/src/cpu/ReferenceRenderer.cpp" \
  "$SDK/Examples/Util/Smart_Utils.cpp" \
  -framework Cocoa -framework Foundation -framework Metal \
  -o "$BUNDLE/Contents/MacOS/StellarGradient"

xcrun Rez -useDF -script Roman \
  -d SystemSevenOrLater=1 -d __MACH__ -d AE_OS_MAC=1 -arch "$ARCH" \
  -i "$SDK/Examples/Headers" -i "$SDK/Examples/Headers/SP" -i "$SDK/Examples/Util" -i "$SDK/Examples/Resources" \
  "$ROOT/src/plugin/StellarGradientPiPL.r" \
  -o "$BUNDLE/Contents/Resources/StellarGradient.rsrc"

plutil -lint "$BUNDLE/Contents/Info.plist" >/dev/null
codesign --force --deep --sign - --timestamp=none "$BUNDLE" >/dev/null
codesign --verify --deep --strict "$BUNDLE"
file "$BUNDLE/Contents/MacOS/StellarGradient"

echo "$BUNDLE"
