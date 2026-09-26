#!/bin/zsh
set -euo pipefail
if [[ "$(uname -s)" != "Darwin" ]]; then echo "Run on the Mac used for AE benchmark"; exit 2; fi

echo "=== Stellar Gradient benchmark environment ==="
sw_vers
printf 'arch: '; uname -m
printf 'cpu: '; sysctl -n machdep.cpu.brand_string 2>/dev/null || sysctl -n hw.model 2>/dev/null || true
printf 'memory_bytes: '; sysctl -n hw.memsize 2>/dev/null || true
system_profiler SPDisplaysDataType 2>/dev/null | grep -E 'Chipset Model:|Chip:|Metal Support:|VRAM' || true

USER_PLUGINS="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
SYSTEM_PLUGINS="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
for p in "$USER_PLUGINS/StellarGradient.plugin" "$SYSTEM_PLUGINS/StellarGradient.plugin"; do [[ -e "$p" ]] && echo "Stellar: $p"; done
for d in "$USER_PLUGINS" "$SYSTEM_PLUGINS"; do
  [[ -d "$d" ]] || continue
  find "$d" -maxdepth 2 \( -iname 'Cosmic.plugin' -o -iname '*Cosmic*.plugin' \) -print 2>/dev/null || true
done

AE_APPS=(/Applications/Adobe\ After\ Effects*/Adobe\ After\ Effects*.app(N))
if (( ${#AE_APPS[@]} )); then
  for app in $AE_APPS; do
    echo "AE: $app"
    defaults read "$app/Contents/Info" CFBundleShortVersionString 2>/dev/null || true
  done
else
  echo "WARN: After Effects not found under /Applications"
fi
