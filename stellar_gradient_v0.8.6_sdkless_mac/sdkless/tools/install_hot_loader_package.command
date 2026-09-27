#!/bin/zsh
set -euo pipefail
setopt null_glob

HERE="${0:A:h}"
BUNDLE="$HERE/StellarGradient.plugin"
SYSTEM_ROOT="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
USER_ROOT="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
STAGED_DIR="$HOME/Library/Application Support/AE Hot Loader/implementations/stellar-gradient"

[[ -d "$BUNDLE" ]] || { echo "ERROR: missing $BUNDLE"; exit 2; }

if pgrep -x "After Effects" >/dev/null 2>&1; then
  echo "ERROR: After Effects is running. Fully quit AE before installing."
  exit 3
fi

typeset -a matches
for root in "$SYSTEM_ROOT" "$USER_ROOT"; do
  [[ -d "$root" ]] || continue
  while IFS= read -r found; do
    matches+=("$found")
  done < <(find "$root" -type d -name "StellarGradient.plugin" -prune -print 2>/dev/null)
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

BACKUP="${TARGET}.pre-ae-hot-loader"
USE_SUDO=0
[[ "$TARGET" == /Library/* ]] && USE_SUDO=1
BACKUP_MADE=0

restore_on_error() {
  local rc=$?
  if (( rc != 0 && BACKUP_MADE == 1 )); then
    echo "Install failed; restoring previous StellarGradient.plugin..."
    if (( USE_SUDO )); then
      sudo rm -rf "$TARGET"
      sudo mv "$BACKUP" "$TARGET"
    else
      rm -rf "$TARGET"
      mv "$BACKUP" "$TARGET"
    fi
  fi
  exit $rc
}
trap restore_on_error EXIT

if (( USE_SUDO )); then
  sudo mkdir -p "${TARGET:h}"
  sudo rm -rf "$BACKUP"
  if [[ -d "$TARGET" ]]; then
    sudo mv "$TARGET" "$BACKUP"
    BACKUP_MADE=1
  fi
  sudo cp -R "$BUNDLE" "$TARGET"
  sudo xattr -dr com.apple.quarantine "$TARGET" 2>/dev/null || true
else
  mkdir -p "${TARGET:h}"
  rm -rf "$BACKUP"
  if [[ -d "$TARGET" ]]; then
    mv "$TARGET" "$BACKUP"
    BACKUP_MADE=1
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
