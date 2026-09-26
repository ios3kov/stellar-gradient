#!/bin/zsh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${AE_SDK_PATH:-${1:-}}"
FAIL=0

ok()   { printf 'OK   %s\n' "$1"; }
warn() { printf 'WARN %s\n' "$1"; }
fail() { printf 'FAIL %s\n' "$1"; FAIL=1; }

[[ "$(uname -s)" == "Darwin" ]] && ok "macOS detected ($(uname -m))" || fail "Run this on macOS"
command -v xcrun >/dev/null 2>&1 && ok "Xcode command line tools" || fail "xcrun not found; install Xcode"
xcrun -f clang++ >/dev/null 2>&1 && ok "clang++" || fail "clang++ missing"
xcrun -f Rez >/dev/null 2>&1 && ok "Rez" || fail "Rez missing"
command -v codesign >/dev/null 2>&1 && ok "codesign" || fail "codesign missing"

if [[ -n "$SDK" && -f "$SDK/Examples/Headers/AE_Effect.h" ]]; then
  ok "After Effects SDK: $SDK"
else
  fail "Set AE_SDK_PATH to the After Effects SDK root (must contain Examples/Headers/AE_Effect.h)"
fi

DEST="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
mkdir -p "$DEST" 2>/dev/null || true
[[ -w "$DEST" ]] && ok "Writable plug-in folder: $DEST" || fail "Plug-in folder is not writable: $DEST"

AE_APPS=(/Applications/Adobe\ After\ Effects*/Adobe\ After\ Effects*.app(N))
if (( ${#AE_APPS[@]} )); then
  for app in $AE_APPS; do ok "Found host: $app"; done
else
  warn "After Effects app was not found under /Applications"
fi

python3 "$ROOT/tools/verify_metadata.py" && ok "PiPL/code metadata agree" || fail "PiPL/code metadata mismatch"
python3 "$ROOT/tools/verify_metal_perf.py" && ok "Metal source contract" || fail "Metal source contract failed"
python3 "$ROOT/tools/verify_ae_contract.py" && ok "AE SmartFX/MFR/cache contract" || fail "AE contract failed"
python3 "$ROOT/tools/verify_code_freeze.py" && ok "Code-freeze contract" || fail "Code-freeze contract failed"

if (( FAIL )); then
  exit 1
fi
printf '\nReady. Build/install with:\n  ./tools/install_mac.sh\n'
