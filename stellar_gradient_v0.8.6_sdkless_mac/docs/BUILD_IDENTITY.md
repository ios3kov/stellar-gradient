# v0.9.6 build identity and package safeguards

2026-09-28. Scope: native host metadata and packaging, not render-algorithm changes.

## Defect and acceptance

Baseline v0.9.5 About reported 0.9.5 but Cargo and PiPL still identified 0.8.8. A version label could not identify the loaded source. New acceptance: one Cargo version feeds PiPL and About; compiled Build ID binds commit, actual source fingerprint, source state, target and CI run/attempt; package metadata matches that binary; dirty/unknown/derived builds cannot enter the CI package gate.

`build.rs` invokes `tools/build_identity.py` using the existing Python 3 prerequisite. It writes generated JSON only under Cargo OUT_DIR and emits `cargo:rustc-env` values used by About. No source files are generated or rewritten. A forced identity check on every incremental build prevents an unchanged Rust file from retaining the previous Git commit identity; this intentionally favors correctness over avoiding a small pre-build check. Release builds remain clean/fresh.

The source digest includes host source, C++/Metal inputs, dependency lock and identity generator. Toolchain versions are recorded, not assumed. The GitHub runner/Rust stable channel are not bitwise-reproducibility guarantees; pinning the full hosted toolchain remains a release limitation.

`package_identity.py --stamp` is allowed only before codesign. It rejects derived/dirty/unknown source and verifies the compiled Build ID in the binary. After codesign, verification and SHA-256 manifest creation are read-only. The final ZIP hash is computed after signing and archiving. No file stores its own hash.

## Tests fixed before candidate validation

10 isolated Python tests: clean/deterministic source identity; tracked changes; untracked files; absent Git history; ignored/reformatted copy; absent lock and malformed metadata; read-only signed-payload manifest; wrong commit/binary; wrong version; refusal to restamp a signed bundle.

Rust tests cover existing ABI, 11 distinct palettes, invalid/separator/Custom choices, default palette and compiled identity/About length. These tests do NOT establish original Cosmic palette values or host UI behavior.

Local Python tests: PASS 10/10. Local unchanged core strict regression: PASS 7/7. Final macOS compile/check/clippy and package verification require the current CI run. Actual AE load/runtime About confirmation remains BLOCKED: this environment has no connected After Effects host.

## Compatibility and limitations

No render math, default values, UI ordering or persistent parameter IDs changed. The old installer still creates a reformatted copy and installs automatically; it is not approved by this candidate's release gate. No user installation is performed. Candidate signing is ad-hoc, not Developer ID notarization. Runtime identity shown by real AE, clean install/restart, migration and full preset/output parity still require actual host evidence.

Research: https://doc.rust-lang.org/cargo/reference/build-scripts.html (OUT_DIR, rustc-env and invalidation); https://docs.github.com/en/actions/tutorials/store-and-share-data (immutable artifacts, hashes). No additional third-party runtime library is introduced.
