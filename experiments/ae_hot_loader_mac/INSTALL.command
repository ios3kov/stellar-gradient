#!/bin/zsh
set -euo pipefail

HERE="${0:A:h}"
PLUGIN="$HERE/AEHotLoader.plugin"
DEST="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"

if [[ ! -d "$PLUGIN" ]]; then
  echo "AEHotLoader.plugin not found next to this installer."
  exit 2
fi

mkdir -p "$DEST"
rm -rf "$DEST/AEHotLoader.plugin"
cp -R "$PLUGIN" "$DEST/AEHotLoader.plugin"
xattr -dr com.apple.quarantine "$DEST/AEHotLoader.plugin" 2>/dev/null || true

echo "Installed:"
echo "$DEST/AEHotLoader.plugin"
echo
echo "Start After Effects, apply AE Hot Loader, then click Register Late Effect."
