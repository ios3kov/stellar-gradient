#!/usr/bin/env python3
"""Stamp an unsigned bundle or verify a signed one without modifying its bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import sys


def validate(bundle, expected_commit=None):
    bundle = Path(bundle)
    identity = json.loads((bundle / "Contents/Resources/stellar-build.json").read_text())
    if identity.get("schema") != 1 or identity.get("source_state") != "clean":
        raise ValueError("bundle does not have canonical clean source identity")
    if expected_commit is not None and identity["git_commit"] != expected_commit:
        raise ValueError("bundle commit does not match candidate commit")
    binary = (bundle / "Contents/MacOS/StellarGradient").read_bytes()
    if identity["build_id"].encode() not in binary:
        raise ValueError("runtime Build ID not found in the packaged binary")
    plist = plistlib.loads((bundle / "Contents/Info.plist").read_bytes())
    for key in ("CFBundleShortVersionString", "CFBundleVersion"):
        if plist.get(key) != identity["version"]:
            raise ValueError(key + " disagrees with compiled package version")
    if plist.get("StellarBuildID") != identity["build_id"]:
        raise ValueError("Info.plist Build ID disagrees with compiled identity")
    return identity


def stamp(bundle, identity_path):
    bundle = Path(bundle)
    identity_path = Path(identity_path)
    if (bundle / "Contents/_CodeSignature").exists():
        raise ValueError("refusing to modify a signed bundle; stamp before codesign")
    identity = json.loads(identity_path.read_text())
    if identity.get("source_state") != "clean":
        raise ValueError("do not package a dirty, derived or unidentified build")
    resources = bundle / "Contents/Resources"
    resources.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(identity_path, resources / "stellar-build.json")
    plist_path = bundle / "Contents/Info.plist"
    plist = plistlib.loads(plist_path.read_bytes())
    plist.update(CFBundleVersion=identity["version"], CFBundleShortVersionString=identity["version"],
                 StellarBuildID=identity["build_id"], StellarGitCommit=identity["git_commit"])
    plist_path.write_bytes(plistlib.dumps(plist))
    return validate(bundle, identity["git_commit"])


def write_manifest(bundle, destination, expected_commit):
    bundle = Path(bundle)
    identity = validate(bundle, expected_commit)
    files = {}
    for path in sorted(bundle.rglob("*")):
        if path.is_symlink():
            raise ValueError("unexpected symlink in plugin payload")
        if path.is_file():
            files[path.relative_to(bundle).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    manifest = {"identity": identity, "payload_sha256": files,
                "release_status": "NOT_APPROVED_AE_GATES_PENDING"}
    Path(destination).write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--stamp", type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--commit")
    args = parser.parse_args()
    if args.stamp:
        stamp(args.bundle, args.stamp)
    if args.manifest:
        write_manifest(args.bundle, args.manifest, args.commit)
    else:
        validate(args.bundle, args.commit)
    print("PASS: bundle version, runtime identity and payload association")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as exc:
        print("Package identity failed: " + str(exc), file=sys.stderr)
        sys.exit(1)
