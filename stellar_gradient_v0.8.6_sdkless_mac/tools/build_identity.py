#!/usr/bin/env python3
"""Generate native-host provenance into Cargo OUT_DIR; never edit source files."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def command(cwd, *args):
    try:
        result = subprocess.run(args, cwd=str(cwd), capture_output=True, text=True,
                                timeout=15, check=False)
    except (OSError, subprocess.TimeoutExpired):
        return None
    return result.stdout.strip() if result.returncode == 0 else None


def collect(crate, version, target, run_id="local", require_clean=False):
    crate = Path(crate).resolve()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("a numeric three-component package version is required")
    for name, value in (("target", target), ("run ID", run_id)):
        if not re.fullmatch(r"[A-Za-z0-9_.-]{1,80}", value):
            raise ValueError("invalid " + name)
    project = crate.parent
    commit = command(crate, "git", "rev-parse", "HEAD")
    status = command(crate, "git", "status", "--porcelain", "--untracked-files=normal")
    if commit is None or not re.fullmatch(r"[0-9a-f]{40}", commit) or status is None:
        commit, state = "unknown", "unknown"
    else:
        state = "dirty" if status else "clean"
    # A reformatted copy is not the canonical tracked crate, even when ignored.
    tracked = command(crate, "git", "ls-files", "--error-unmatch", "Cargo.toml")
    if tracked != "Cargo.toml" and state == "clean":
        state = "derived"
    if require_clean and state != "clean":
        raise ValueError("release requires canonical clean Git source, got " + state)
    if not (crate / "Cargo.lock").is_file():
        raise ValueError("Cargo.lock is required; do not resolve release dependencies implicitly")
    inputs = {}
    for base, label in ((crate / "src", "sdkless/src"),
                        (crate / "bridge", "sdkless/bridge"),
                        (project / "src", "src")):
        for path in sorted(base.rglob("*")):
            if path.is_file():
                inputs[label + "/" + path.relative_to(base).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    for path, label in ((crate / "Cargo.toml", "sdkless/Cargo.toml"),
                        (crate / "Cargo.lock", "sdkless/Cargo.lock"),
                        (crate / "build.rs", "sdkless/build.rs"),
                        (Path(__file__), "tools/build_identity.py")):
        inputs[label] = hashlib.sha256(path.read_bytes()).hexdigest()
    digest = hashlib.sha256(json.dumps(inputs, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    build_id = "sg-{}-{}-{}-{}-{}-{}".format(version, commit[:12], state, digest[:12], target, run_id)
    return {"schema": 1, "version": version, "git_commit": commit,
            "source_state": state, "source_sha256": digest, "target": target,
            "build_id": build_id, "test_run_id": run_id,
            "rustc": command(crate, "rustc", "--version") or "unavailable",
            "cargo": command(crate, "cargo", "--version") or "unavailable",
            "macos_sdk": command(crate, "xcrun", "--show-sdk-version") or "unavailable",
            "source_files": inputs}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--crate", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    run_id = os.environ.get("GITHUB_RUN_ID", "local") + "." + os.environ.get("GITHUB_RUN_ATTEMPT", "1")
    identity = collect(args.crate, args.version, args.target, run_id,
                       os.environ.get("SG_REQUIRE_CLEAN_BUILD") == "1")
    args.out.mkdir(parents=True, exist_ok=True)
    tmp = args.out / "stellar-build.json.tmp"
    tmp.write_text(json.dumps(identity, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    tmp.replace(args.out / "stellar-build.json")
    for key, field in (("SG_BUILD_ID", "build_id"), ("SG_BUILD_COMMIT", "git_commit"),
                       ("SG_SOURCE_STATE", "source_state"), ("SG_BUILD_TARGET", "target")):
        print("cargo:rustc-env=" + key + "=" + identity[field])


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as exc:
        print("Build identity failed: " + str(exc), file=sys.stderr)
        sys.exit(1)
