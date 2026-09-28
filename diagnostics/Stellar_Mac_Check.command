#!/bin/bash
# Stellar install check 1.0. Read-only inspection; not an installer or AE test.
# Requires only macOS built-in commands. No sudo, downloads or preference edits.
# Can be sourced by the fixture tests; normal execution always uses the real OS.

sg_run() { "$@"; }
sg_line() {
    local message="$*"
    message=${message//"$HOME"/\<HOME\>}
    # %q prevents filenames/control characters from injecting report entries.
    printf '%q\n' "$message"
}
sg_capture() {
    local label="$1" result rc
    shift
    result=$(sg_run "$@" 2>&1); rc=$?
    sg_line "$label exit=$rc ${result:0:3000}"
    return "$rc"
}
sg_root() {
    local candidate="$1" old
    for old in "${SG_ROOTS[@]}"; do [[ "$old" == "$candidate" ]] && return; done
    SG_ROOTS+=("$candidate")
}
sg_discover() {
    local base parent app pid exe
    sg_root '/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore'
    sg_root "$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
    for base in /Applications "$HOME/Applications"; do
        for parent in "$base"/Adobe\ After\ Effects*; do
            [[ -d "$parent" ]] || continue
            if [[ "$parent" == *.app ]]; then
                sg_root "$parent/Contents/Plug-ins"
            else
                sg_root "$parent/Plug-ins"
                for app in "$parent"/Adobe\ After\ Effects*.app; do
                    [[ -d "$app" ]] && sg_root "$app/Contents/Plug-ins"
                done
            fi
        done
    done
    # Only process names/paths, never command-line arguments or project names.
    # Exact running app location also covers nonstandard application installs.
    while read -r pid exe; do
        [[ "$pid" =~ ^[0-9]+$ ]] || continue
        case "$exe" in
            *After\ Effects*.app/Contents/MacOS/*)
                SG_PIDS+=("$pid")
                SG_APPS+=("${exe%/Contents/MacOS/*}")
                app=${exe%/Contents/MacOS/*}
                sg_root "${app%/*}/Plug-ins"
                sg_root "$app/Contents/Plug-ins"
                ;;
        esac
    done < <(sg_run /bin/ps -U "$(sg_run /usr/bin/id -u)" -o pid=,comm= 2>/dev/null)
}
sg_bundle() {
    local bundle="$1" origin="$2" disabled="$3" plist executable binary attrs result rc
    if (( SG_BUNDLES >= 32 )); then SG_PARTIAL=1; return; fi
    SG_BUNDLES=$((SG_BUNDLES + 1))
    [[ "$origin" == installed-location ]] && SG_INSTALLED=$((SG_INSTALLED + 1))
    sg_line "BUNDLE $SG_BUNDLES location=$origin disabled_directory_hint=$disabled path=$bundle"
    if [[ -L "$bundle" ]]; then
        SG_PARTIAL=1
        sg_line 'SYMLINK: not traversed; target and loadability NOT_VERIFIED'
        return
    fi
    plist="$bundle/Contents/Info.plist"
    if [[ ! -f "$plist" || -L "$plist" || -L "$bundle/Contents" ]]; then
        sg_line 'STRUCTURE: missing plist or symlink; NOT_VERIFIED'
        SG_PARTIAL=1; return
    fi
    local key
    for key in CFBundleIdentifier CFBundleShortVersionString CFBundleVersion StellarBuildID StellarGitCommit; do
        sg_capture "PLIST $key" /usr/libexec/PlistBuddy -c "Print :$key" "$plist" || :
    done
    executable=$(sg_run /usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$plist" 2>/dev/null)
    case "$executable" in ''|.|..|*/*|*$'\n'*)
        sg_line 'STRUCTURE: invalid executable name; no binary inspected'; SG_PARTIAL=1; return ;;
    esac
    binary="$bundle/Contents/MacOS/$executable"
    if [[ ! -f "$binary" || -L "$binary" || -L "$bundle/Contents/MacOS" ]]; then
        sg_line 'STRUCTURE: executable absent or symlink; NOT_VERIFIED'
        SG_PARTIAL=1; return
    fi
    sg_capture 'BINARY FORMAT (file on disk, not running AE)' /usr/bin/file -b "$binary" || :
    sg_capture 'BINARY SHA256' /usr/bin/shasum -a 256 "$binary" || :
    if sg_run /usr/bin/grep -aFq 'AEHotLoader_ShellReload' "$binary"; then
        sg_line 'SHELL MARKER: found; do not replace with standalone without migration review'
    else
        sg_line 'SHELL MARKER: not found (not proof of standalone version)'
    fi
    attrs=$(sg_run /usr/bin/xattr "$bundle" 2>/dev/null); rc=$?
    if [[ "$rc" != 0 ]]; then
        sg_line 'QUARANTINE bundle: NOT_VERIFIED'
    elif [[ $'\n'"$attrs"$'\n' == *$'\ncom.apple.quarantine\n'* ]]; then
        sg_line 'QUARANTINE bundle: present; not removed (not proof of a rejection)'
    else
        sg_line 'QUARANTINE bundle: absent (not proof of Gatekeeper approval)'
    fi
    attrs=$(sg_run /usr/bin/xattr "$binary" 2>/dev/null); rc=$?
    if [[ "$rc" != 0 ]]; then
        sg_line 'QUARANTINE executable: NOT_VERIFIED'
    elif [[ $'\n'"$attrs"$'\n' == *$'\ncom.apple.quarantine\n'* ]]; then
        sg_line 'QUARANTINE executable: present; not removed'
    else
        sg_line 'QUARANTINE executable: absent'
    fi
    sg_capture 'CODESIGN static verify (not AE/Gatekeeper approval)' /usr/bin/codesign --verify --strict "$bundle" || :
    # Only signature classification, not signer identity or embedded entitlements.
    result=$(sg_run /usr/bin/codesign -dv "$bundle" 2>&1)
    case "$result" in *'Signature=adhoc'*) sg_line 'SIGNATURE: ad-hoc' ;; esac
}
sg_walk() {
    local dir="$1" depth="$2" origin="$3" disabled="$4" child name mark
    if (( SG_VISITED >= 2500 )); then SG_PARTIAL=1; return; fi
    SG_VISITED=$((SG_VISITED + 1))
    if [[ -L "$dir" || ! -r "$dir" || ! -x "$dir" ]]; then
        SG_PARTIAL=1; sg_line "SKIPPED unreadable/symlink directory: $dir"; return
    fi
    for child in "$dir"/*; do
        name=${child##*/}; mark="$disabled"
        case "$name" in \(*\)|¬*) mark=yes ;; esac
        case "$name" in
            *[Ss][Tt][Ee][Ll][Ll][Aa][Rr]*.plugin)
                sg_bundle "$child" "$origin" "$mark"; continue ;;
            *[Ss][Tt][Ee][Ll][Ll][Aa][Rr]*.aex)
                sg_line "WINDOWS-EXTENSION FILE location=$origin path=$child"
                sg_capture 'FORMAT' /usr/bin/file -b "$child" || :; continue ;;
            *[Ss][Tt][Ee][Ll][Ll][Aa][Rr]*.zip)
                sg_line "ARCHIVE ONLY (not installed by this check): $child"; continue ;;
            *.plugin|*.app|.git|node_modules) continue ;;
        esac
        [[ -d "$child" || -L "$child" ]] || continue
        if (( depth <= 0 )); then
            SG_PARTIAL=1; continue
        fi
        sg_walk "$child" "$((depth - 1))" "$origin" "$mark"
    done
}
sg_report() {
    local root i pid mappings rc arch
    sg_line 'Stellar Mac Check 1.0; read-only installation inventory, not release approval'
    sg_line "RUN $(sg_run /bin/date -u +%Y%m%dT%H%M%SZ) ${SG_OUTPUT##*/}"
    sg_line 'TARGET version=0.9.6 commit=deec78835801c5bf6f1aa44c772b508e43697b11; installed version NOT_ASSUMED'
    sg_capture 'COLLECTOR SHA256' /usr/bin/shasum -a 256 "${BASH_SOURCE[0]}" || :
    sg_capture 'macOS' /usr/bin/sw_vers -productVersion || :
    sg_capture 'Terminal process architecture (not AE architecture)' /usr/bin/uname -m || :
    sg_capture 'Hardware supports arm64' /usr/sbin/sysctl -n hw.optional.arm64 || :
    sg_discover
    for root in "${SG_ROOTS[@]}"; do
        if [[ -e "$root" || -L "$root" ]]; then
            sg_line "SCAN ROOT $root"
            sg_walk "$root" 10 installed-location no
        else
            sg_line "ROOT NOT PRESENT $root"
        fi
    done
    sg_line "INSTALLED-LOCATION named_bundles=$SG_INSTALLED partial_scan=$SG_PARTIAL"
    # Downloads are only a shallow staging check, never an install location.
    SG_PARTIAL=0
    if [[ -d "$HOME/Downloads" ]]; then
        sg_walk "$HOME/Downloads" 2 download-staging no
    fi
    sg_line "DOWNLOAD scan_partial=$SG_PARTIAL"
    for ((i=0; i<${#SG_PIDS[@]}; i++)); do
        pid=${SG_PIDS[$i]}
        sg_line "RUNNING AE pid=$pid app=${SG_APPS[$i]}"
        arch=$(sg_run /bin/ps -p "$pid" -o arch= 2>/dev/null)
        case "$arch" in *arm64*|*x86_64*|*i386*) sg_line "AE PROCESS ARCH: $arch" ;;
            *) sg_line 'AE PROCESS ARCH: NOT_VERIFIED' ;; esac
        # Filter in memory; unrelated open files and project paths are never saved.
        mappings=$(sg_run /usr/sbin/lsof -n -P -a -p "$pid" -Fn 2>/dev/null); rc=$?
        if [[ "$rc" != 0 ]]; then
            sg_line 'STELLAR MAPPING: NOT_VERIFIED (lsof unavailable/denied)'
        else
            mappings=$(printf '%s\n' "$mappings" | /usr/bin/grep -i '^n.*stellar.*\.plugin/Contents/' | /usr/bin/head -n 20)
            sg_line "STELLAR MAPPING observation only: ${mappings:-none visible}"
        fi
    done
    sg_line 'NOT_VERIFIED: Effects Manager enablement, nonstandard unopened app paths, runtime Build ID and actual render'
    sg_line 'No plugin was installed/removed/executed; no cache, preference or security setting changed; no process stopped.'
    sg_line 'Read-only commands have no hard timeout; Ctrl+C safely stops collection; a partial report is not PASS.'
}
sg_main() (
    # Subshell keeps option changes local to this diagnostic.
    set +e
    umask 077
    export LC_ALL=C
    shopt -s nullglob dotglob
    if [[ $(sg_run /usr/bin/uname -s) != Darwin ]]; then
        printf '%s\n' 'This check runs only on macOS; nothing was changed.' >&2; return 2
    fi
    if [[ -z "$HOME" || "$HOME" != /* || ! -d "$HOME/Desktop" ]]; then
        printf '%s\n' 'Desktop folder unavailable; no report or host changes made.' >&2; return 3
    fi
    SG_ROOTS=(); SG_PIDS=(); SG_APPS=()
    SG_PARTIAL=0; SG_VISITED=0; SG_BUNDLES=0; SG_INSTALLED=0
    SG_OUTPUT=$(sg_run /usr/bin/mktemp -d "$HOME/Desktop/Stellar-Mac-Check.XXXXXXXX") || return 3
    printf '%s\n' 'Checking Stellar files only. After Effects can stay open.'
    sg_report > "$SG_OUTPUT/report.txt" 2>&1
    printf '\n%s\n%s\n' 'Report created. Send report.txt from this folder:' "$SG_OUTPUT"
)
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then sg_main "$@"; fi
