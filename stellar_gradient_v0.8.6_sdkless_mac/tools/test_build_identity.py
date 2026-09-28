#!/usr/bin/env python3
"""Isolated tests for clean-source and immutable-bundle release safeguards."""
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile
import unittest

from build_identity import collect
from package_identity import stamp, validate, write_manifest


class IdentityTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="stellar-identity-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.crate = self.root / "project/sdkless"
        self.crate.mkdir(parents=True)
        for name in ("Cargo.toml", "Cargo.lock", "build.rs"):
            (self.crate / name).write_text("fixture\n")
        self.git("init", "-q")
        self.git("config", "maintenance.auto", "false")
        self.git("config", "gc.auto", "0")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("config", "user.name", "Identity test")
        self.git("add", ".")
        self.git("commit", "-qm", "fixture")

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root)] + list(args), text=True).strip()

    def identity(self, **kwargs):
        return collect(self.crate, "0.9.6", "aarch64-apple-darwin", **kwargs)

    def bundle(self):
        record = self.identity(require_clean=True)
        bundle = self.root / "StellarGradient.plugin"
        (bundle / "Contents/MacOS").mkdir(parents=True)
        (bundle / "Contents/MacOS/StellarGradient").write_bytes(record["build_id"].encode())
        (bundle / "Contents/Info.plist").write_bytes(plistlib.dumps({}))
        source = self.root / "identity.json"
        source.write_text(json.dumps(record))
        stamp(bundle, source)
        return bundle, record, source

    def test_clean_commit_and_repeatable_identity(self):
        a = self.identity(require_clean=True)
        self.assertEqual(a["git_commit"], self.git("rev-parse", "HEAD"))
        self.assertEqual(a["source_state"], "clean")
        self.assertEqual(a["build_id"], self.identity(require_clean=True)["build_id"])

    def test_dirty_source_is_rejected(self):
        old = self.identity()
        (self.crate / "build.rs").write_text("changed\n")
        self.assertEqual(self.identity()["source_state"], "dirty")
        self.assertNotEqual(old["source_sha256"], self.identity()["source_sha256"])
        with self.assertRaisesRegex(ValueError, "clean Git"):
            self.identity(require_clean=True)

    def test_untracked_source_is_not_clean(self):
        (self.crate / "unexpected.rs").write_text("unexpected")
        with self.assertRaises(ValueError):
            self.identity(require_clean=True)

    def test_archive_has_no_verified_commit(self):
        shutil.rmtree(self.root / ".git")
        self.assertEqual(self.identity()["source_state"], "unknown")
        with self.assertRaises(ValueError):
            self.identity(require_clean=True)

    def test_ignored_copy_is_derived_not_clean(self):
        (self.root / ".gitignore").write_text("project/.sdkless-ci/\n")
        self.git("add", ".gitignore")
        self.git("commit", "-qm", "ignore test copy")
        copy = self.crate.parent / ".sdkless-ci"
        shutil.copytree(self.crate, copy)
        self.assertEqual(collect(copy, "0.9.6", "aarch64-apple-darwin")["source_state"], "derived")
        with self.assertRaises(ValueError):
            collect(copy, "0.9.6", "aarch64-apple-darwin", require_clean=True)

    def test_missing_lock_and_injected_values_are_rejected(self):
        with self.assertRaises(ValueError):
            self.identity(run_id="bad\ncargo:rustc-env=INJECT=1")
        (self.crate / "Cargo.lock").unlink()
        with self.assertRaisesRegex(ValueError, "Cargo.lock"):
            self.identity()

    def test_signed_payload_manifest_is_read_only(self):
        bundle, record, _ = self.bundle()
        signature = bundle / "Contents/_CodeSignature"
        signature.mkdir()
        (signature / "CodeResources").write_bytes(b"signature-fixture")
        before = {str(p): p.read_bytes() for p in bundle.rglob("*") if p.is_file()}
        manifest = write_manifest(bundle, self.root / "manifest.json", record["git_commit"])
        self.assertIn("Contents/_CodeSignature/CodeResources", manifest["payload_sha256"])
        self.assertEqual(before, {str(p): p.read_bytes() for p in bundle.rglob("*") if p.is_file()})

    def test_wrong_commit_and_binary_are_rejected(self):
        bundle, _, _ = self.bundle()
        with self.assertRaises(ValueError):
            validate(bundle, "0" * 40)
        (bundle / "Contents/MacOS/StellarGradient").write_bytes(b"old binary")
        with self.assertRaisesRegex(ValueError, "runtime Build ID"):
            validate(bundle)

    def test_version_mismatch_is_rejected(self):
        bundle, _, _ = self.bundle()
        path = bundle / "Contents/Info.plist"
        plist = plistlib.loads(path.read_bytes())
        plist["CFBundleVersion"] = "0.8.8"
        path.write_bytes(plistlib.dumps(plist))
        with self.assertRaisesRegex(ValueError, "version"):
            validate(bundle)

    def test_signed_bundle_cannot_be_restamped(self):
        bundle, _, source = self.bundle()
        (bundle / "Contents/_CodeSignature").mkdir()
        with self.assertRaisesRegex(ValueError, "signed bundle"):
            stamp(bundle, source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
