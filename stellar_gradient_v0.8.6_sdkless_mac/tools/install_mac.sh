#!/bin/zsh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUNDLE="$($ROOT/tools/build_mac.sh "${AE_SDK_PATH:-${1:-}}")"
DEST="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
mkdir -p "$DEST"
rm -rf "$DEST/StellarGradient.plugin"
cp -R "$BUNDLE" "$DEST/"
echo "Installed: $DEST/StellarGradient.plugin"
echo "Restart After Effects -> Effect > Stellar > Stellar Gradient"
