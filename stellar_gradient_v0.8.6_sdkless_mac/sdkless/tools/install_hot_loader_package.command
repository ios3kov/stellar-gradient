#!/bin/zsh
set -euo pipefail
setopt null_glob

HERE="${0:A:h}"
BUNDLE="$HERE/StellarGradient.plugin"
SYSTEM_ROOT="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
USER_ROOT="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
STAGED_DIR="$HOME/Library/Application Support/AE Hot Loader/implementations/stellar-gradient"

[[ -d "$BUNDLE" ]] || { echo "ERROR: missing $BUNDLE"; exit 2; }

codesign --verify --deep --strict "$BUNDLE"
bundle_archs="$(lipo -archs "$BUNDLE/Contents/MacOS/StellarGradient" 2>/dev/null || true)"
[[ "$bundle_archs" == *arm64* ]] || { echo "ERROR: Stellar Gradient shell is not arm64."; exit 2; }

if pgrep -x "After Effects" >/dev/null 2>&1; then
  echo "ERROR: After Effects is running. Fully quit AE before installing."
  exit 3
fi

typeset -a search_roots
search_roots=(
  "$SYSTEM_ROOT"
  "$USER_ROOT"
  "/Library/Application Support/Adobe/Plug-Ins/CC"
  "$HOME/Library/Application Support/Adobe/Plug-Ins/CC"
)
for app_plugins in /Applications/Adobe\ After\ Effects*.app/Contents/Plug-ins; do
  [[ -d "$app_plugins" ]] && search_roots+=("$app_plugins")
done

typeset -a matches
typeset -A match_seen

record_match() {
  local found="$1"
  if [[ -z "${match_seen[$found]-}" ]]; then
    match_seen[$found]=1
    matches+=("$found")
  fi
}

for root in "${search_roots[@]}"; do
  [[ -d "$root" ]] || continue

  while IFS= read -r found; do
    record_match "$found"
  done < <(find "$root" -type d -name "StellarGradient.plugin" -prune -print 2>/dev/null)

  while IFS= read -r found; do
    plist="$found/Contents/Info.plist"
    [[ -f "$plist" ]] || continue
    found_id="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$plist" 2>/dev/null || true)"
    if [[ "$found_id" == "com.stellarlabs.StellarGradient" ]]; then
      record_match "$found"
    fi
  done < <(find "$root" -type d -name "*.plugin" -prune -print 2>/dev/null)
done

if (( ${#matches[@]} > 1 )); then
  echo "ERROR: multiple StellarGradient.plugin copies found:"
  for found in "${matches[@]}"; do echo "  $found"; done
  echo "Resolve duplicates first; installer will not guess which copy AE should load."
  exit 4
fi

if (( ${#matches[@]} == 1 )); then
  TARGET="${matches[1]}"
else
  TARGET="$USER_ROOT/StellarGradient.plugin"
fi

if [[ "$TARGET" == /Applications/*.app/Contents/Plug-ins/* ]]; then
  echo "ERROR: existing Stellar Gradient copy is inside the signed After Effects application bundle:"
  echo "  $TARGET"
  echo "Do not modify the Adobe app bundle. Remove/relocate that custom copy first."
  exit 5
fi

BACKUP_ROOT="$HOME/Library/Application Support/AE Hot Loader/backups/stellar-gradient"
mkdir -p "$BACKUP_ROOT"
BACKUP="$BACKUP_ROOT/StellarGradient-$(date +%Y%m%dT%H%M%S)-$$.plugin"
USE_SUDO=0
[[ "$TARGET" == /Library/* ]] && USE_SUDO=1
BACKUP_MADE=0

restore_on_error() {
  local rc=$?
  if (( rc != 0 )); then
    echo "Install failed; cleaning partial StellarGradient.plugin..."
    if (( USE_SUDO )); then
      sudo rm -rf "$TARGET"
      if (( BACKUP_MADE == 1 )); then
        sudo cp -R "$BACKUP" "$TARGET"
        sudo xattr -dr com.apple.quarantine "$TARGET" 2>/dev/null || true
      fi
    else
      rm -rf "$TARGET"
      if (( BACKUP_MADE == 1 )); then
        cp -R "$BACKUP" "$TARGET"
        xattr -dr com.apple.quarantine "$TARGET" 2>/dev/null || true
      fi
    fi
  fi
  exit $rc
}
trap restore_on_error EXIT

if (( USE_SUDO )); then
  sudo mkdir -p "${TARGET:h}"
  if [[ -d "$TARGET" ]]; then
    cp -R "$TARGET" "$BACKUP"
    BACKUP_MADE=1
    sudo rm -rf "$TARGET"
  fi
  sudo cp -R "$BUNDLE" "$TARGET"
  sudo xattr -dr com.apple.quarantine "$TARGET" 2>/dev/null || true
else
  mkdir -p "${TARGET:h}"
  if [[ -d "$TARGET" ]]; then
    cp -R "$TARGET" "$BACKUP"
    BACKUP_MADE=1
    rm -rf "$TARGET"
  fi
  cp -R "$BUNDLE" "$TARGET"
  xattr -dr com.apple.quarantine "$TARGET" 2>/dev/null || true
fi

codesign --verify --deep --strict "$TARGET"
rm -f "$STAGED_DIR/current.dylib" "$STAGED_DIR/current.tmp.dylib" 2>/dev/null || true

trap - EXIT

echo
echo "Installed hot-reload Stellar Gradient shell:"
echo "  $TARGET"
if (( BACKUP_MADE )); then echo "Backup: $BACKUP"; fi
echo "Open After Effects once. Future implementation updates do not require restart."
