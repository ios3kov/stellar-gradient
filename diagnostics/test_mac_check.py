"""Fixture tests of the actual Bash collector; no AE/user plugin installation."""
import hashlib
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).with_name('Stellar_Mac_Check.command')
HARNESS = r'''
source "$CHECK_SCRIPT"
sg_run() {
    printf '%q ' "$@" >> "$TRACE"; printf '\n' >> "$TRACE"
    case "$1" in
        /usr/bin/uname) [[ "$2" == -s ]] && printf '%s\n' "$MOCK_OS" || echo arm64 ;;
        /usr/bin/sw_vers) echo fixture-not-real-macOS ;;
        /usr/sbin/sysctl) echo 1 ;;
        /usr/libexec/PlistBuddy)
            case "$3" in
                'Print :CFBundleExecutable') printf '%s\n' "$MOCK_EXEC" ;;
                'Print :StellarBuildID') echo fixture-build-not-real ;;
                *) echo fixture-plist-value ;;
            esac ;;
        /usr/bin/codesign)
            [[ "$2" == -dv ]] && { echo Signature=adhoc; return; }
            echo fixture-codesign-not-real; return "$SIGN_RC" ;;
        /usr/bin/xattr) [[ "$QUARANTINE" == yes ]] && echo com.apple.quarantine; return 0 ;;
        /bin/ps) echo '  arm64';;
        /usr/sbin/lsof)
            printf 'n%s\nn%s\n' "$HOME/Secret-project/client.aep" "$HOME/Library/Test/StellarGradient.plugin/Contents/MacOS/StellarGradient"
            return "$LSOF_RC" ;;
        *) "$@" ;;
    esac
}
sg_discover() {
    SG_ROOTS=("$HOME/Library/Test")
    if [[ "$RUNNING" == yes ]]; then
        SG_PIDS=(9876); SG_APPS=("$HOME/Applications/Adobe After Effects.app")
    fi
}
sg_main
'''


class CollectorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='stellar-install-test-')
        self.addCleanup(self.temp.cleanup)
        self.home = Path(self.temp.name)
        (self.home/'Desktop').mkdir()
        (self.home/'Downloads').mkdir()
        (self.home/'Library/Test').mkdir(parents=True)
        self.env = dict(os.environ, HOME=str(self.home), CHECK_SCRIPT=str(SCRIPT),
                        TRACE=str(self.home/'trace'), MOCK_OS='Darwin', MOCK_EXEC='StellarGradient',
                        SIGN_RC='0', QUARANTINE='no', RUNNING='no', LSOF_RC='0')

    def bundle(self, name='StellarGradient.plugin', location='Library/Test', marker=b'fixture only'):
        p = self.home/location/name
        (p/'Contents/MacOS').mkdir(parents=True)
        (p/'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable':'StellarGradient'}))
        (p/'Contents/MacOS/StellarGradient').write_bytes(marker)
        return p

    def run_check(self, **env):
        result = subprocess.run(['/bin/bash', '-c', HARNESS], env=dict(self.env, **env),
                                capture_output=True, text=True, timeout=20)
        reports = sorted((self.home/'Desktop').glob('Stellar-Mac-Check.*/report.txt'),
                         key=lambda p: p.stat().st_mtime_ns)
        text = reports[-1].read_text() if reports else ''
        # Unescape only quoting used for ordinary English report assertions.
        return result, text.replace('\\ ', ' '), reports

    def unchanged(self):
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (self.home/'Library').rglob('*') if p.is_file() and not p.is_symlink()}

    def test_wrong_os_stops_before_write(self):
        result, _, paths = self.run_check(MOCK_OS='Linux')
        self.assertEqual(result.returncode, 2); self.assertEqual(paths, [])

    def test_no_named_bundles_is_not_a_global_absence_claim(self):
        result, text, paths = self.run_check()
        self.assertEqual(result.returncode, 0); self.assertEqual(len(paths), 1)
        self.assertIn('named_bundles=0', text)
        self.assertIn('nonstandard unopened app paths', text)
        self.assertNotIn('release_status=PASS', text)

    def test_candidate_read_and_unrelated_cosmic_not_inspected(self):
        self.bundle(); self.bundle('Cosmic.plugin', marker=b'untouched')
        before = self.unchanged()
        _, text, _ = self.run_check()
        self.assertIn('named_bundles=1', text); self.assertIn('BINARY SHA256 exit=0', text)
        self.assertNotIn('Cosmic', (self.home/'trace').read_text())
        self.assertEqual(before, self.unchanged())

    def test_duplicate_bundles_never_replaced(self):
        self.bundle(); self.bundle(location='Library/Test/old')
        before = self.unchanged()
        _, text, _ = self.run_check()
        self.assertIn('named_bundles=2', text); self.assertEqual(before, self.unchanged())

    def test_disabled_folder_is_labelled(self):
        self.bundle(location='Library/Test/(disabled)')
        _, text, _ = self.run_check()
        self.assertIn('disabled_directory_hint=yes', text)

    def test_bundle_link_not_traversed(self):
        p = self.bundle(location='outside')
        (self.home/'Library/Test/StellarLink.plugin').symlink_to(p, target_is_directory=True)
        _, text, _ = self.run_check()
        self.assertIn('SYMLINK: not traversed', text); self.assertNotIn('BINARY SHA256', text)

    def test_binary_link_not_inspected(self):
        p = self.bundle(); binary = p/'Contents/MacOS/StellarGradient'
        binary.unlink(); binary.symlink_to('/usr/bin/true')
        _, text, _ = self.run_check()
        self.assertIn('executable absent or symlink', text); self.assertNotIn('BINARY SHA256', text)

    def test_malformed_bundle_is_reported(self):
        p = self.bundle(); (p/'Contents/Info.plist').unlink()
        _, text, _ = self.run_check()
        self.assertIn('missing plist', text); self.assertNotIn('BINARY SHA256', text)

    def test_untrusted_plist_cannot_escape_bundle(self):
        self.bundle()
        _, text, _ = self.run_check(MOCK_EXEC='../../../Cosmic.plugin')
        self.assertIn('invalid executable name', text); self.assertNotIn('BINARY SHA256', text)

    def test_signature_failure_is_not_repaired_or_hidden(self):
        self.bundle(); before = self.unchanged()
        _, text, _ = self.run_check(SIGN_RC='1')
        self.assertIn('approval) exit=1', text.replace('\\)', ')').replace('\\(', '('))
        trace = (self.home/'trace').read_text()
        self.assertNotIn('--sign', trace); self.assertEqual(before, self.unchanged())

    def test_quarantine_is_observed_not_removed(self):
        self.bundle()
        _, text, _ = self.run_check(QUARANTINE='yes')
        self.assertIn('QUARANTINE bundle: present', text)
        self.assertNotIn('xattr -d', (self.home/'trace').read_text())

    def test_shell_marker_detected_without_loading_it(self):
        self.bundle(marker=b'AEHotLoader_ShellReload')
        _, text, _ = self.run_check()
        self.assertIn('SHELL MARKER: found', text)

    def test_download_is_not_counted_as_installed(self):
        self.bundle(location='Downloads')
        _, text, _ = self.run_check()
        self.assertIn('named_bundles=0', text); self.assertIn('location=download-staging', text)

    def test_loaded_file_filter_excludes_unrelated_paths(self):
        _, text, _ = self.run_check(RUNNING='yes')
        self.assertIn('STELLAR MAPPING observation only', text)
        self.assertNotIn('Secret-project', text); self.assertNotIn(str(self.home), text)

    def test_mapping_failure_remains_unverified(self):
        _, text, _ = self.run_check(RUNNING='yes', LSOF_RC='1')
        self.assertIn('STELLAR MAPPING: NOT_VERIFIED', text)

    def test_new_reports_do_not_overwrite_old_results(self):
        _, _, first = self.run_check()
        before = first[0].read_bytes()
        _, _, second = self.run_check()
        self.assertEqual(len(second), 2); self.assertEqual(first[0].read_bytes(), before)

    def test_missing_desktop_does_not_create_user_directories(self):
        (self.home/'Desktop').rmdir()
        result, _, reports = self.run_check()
        self.assertEqual(result.returncode, 3); self.assertEqual(reports, [])
        self.assertFalse((self.home/'Desktop').exists())

    def test_control_character_filename_cannot_forge_record(self):
        self.bundle('Stellar\nFAKE_PASS.plugin')
        _, text, _ = self.run_check()
        self.assertNotIn('\nFAKE_PASS', text)

    def test_directory_link_is_not_followed(self):
        self.bundle(location='outside')
        (self.home/'Library/Test/linked').symlink_to(self.home/'outside', target_is_directory=True)
        _, text, _ = self.run_check()
        self.assertIn('partial_scan=1', text); self.assertNotIn('BINARY SHA256', text)

    def test_deep_download_scan_is_marked_partial(self):
        self.bundle(location='Downloads/one/two/three/four')
        _, text, _ = self.run_check()
        self.assertIn('DOWNLOAD scan_partial=1', text)

    @unittest.skipUnless(sys.platform == 'darwin', 'needs actual macOS built-in commands')
    def test_real_mac_tools_on_owned_unsigned_fixture(self):
        self.bundle(); before = self.unchanged()
        runner = 'source "$CHECK_SCRIPT"; sg_discover() { SG_ROOTS=("$HOME/Library/Test"); }; sg_main'
        result = subprocess.run(['/bin/bash', '-c', runner], env=self.env,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = next((self.home/'Desktop').glob('Stellar-Mac-Check.*/report.txt')).read_text().replace('\\ ', ' ')
        self.assertIn('BINARY SHA256 exit=0', report)
        self.assertRegex(report, r'CODESIGN static verify.* exit=[1-9][0-9]*')
        self.assertEqual(before, self.unchanged())


if __name__ == '__main__':
    unittest.main(verbosity=2)
