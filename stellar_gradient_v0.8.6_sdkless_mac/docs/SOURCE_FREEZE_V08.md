# Source Freeze v0.8 — SDK-less Mac Candidate

Date: 2026-09-26
Status: FROZEN CANDIDATE — v0.8.6 build 14

## Version model

- Render core remains the previously audited/frozen **v0.6 core**.
- SDK-less AE host and bridges are **v0.8.6 build 14** after the MFR/cfg compiler hotfix.
- No v0.6 render math was changed during v0.8 host hardening.

## Freeze scope

The v0.8 SHA-256 manifest freezes executable/reproducibility inputs:

- `src/core`, `src/cpu`, `src/gpu`
- complete `sdkless` host/bridge/build layer (excluding generated `target`)
- regression tests
- build/audit/verification tools
- `CMakeLists.txt`
- `FIRST_MAC_BUILD.command`

Documentation files are intentionally outside the hash scope so status notes can be updated after runtime tests without invalidating the code freeze.

## Verification

Run:

```bash
python3 tools/verify_v08_host_freeze.py
```

Any source/build-script modification after this point must be justified by a reproduced Mac/After Effects failure or a measured profiling result, followed by a new manifest.

## v0.8.1 bootstrap hotfix

The first Mac invocation exposed a launcher-only issue: Homebrew Rust 1.80.1 was present, so the old script skipped rustup installation and then called missing `rustup`. The hotfix:

- detects the actual `rustc` version;
- requires Rust >=1.85 for edition 2024;
- uses a sufficiently new complete system toolchain directly;
- otherwise installs/updates user-local stable Rust automatically;
- removes the unconditional `rustup target add` requirement for the native arm64 target.

No render-core math changed.

## v0.8.2 sanitizer hotfix

The second Mac invocation passed source contracts and bridge parity, then stopped because Apple macOS ASan does not support `ASAN_OPTIONS=detect_leaks=1`. The hotfix:

- sets `detect_leaks=0` on the macOS ASan smoke test;
- keeps ASan `halt_on_error=1`;
- keeps UBSan `halt_on_error=1` with stack traces;
- does not modify frozen render math or the C/Rust/Metal ABI.

Leak/resource-lifetime checks remain part of the later real AE/Metal profiling stage using macOS-native tooling.

## v0.8.3 Rust-host compile hotfix

The third Mac invocation passed the frozen-source, native-bridge and sanitizer stages, then the Rust compiler rejected host-only leading-dot float literals (for example `.13` and `.01`). The hotfix:

- converts all Rust fractional literals to the required `0.xx` syntax;
- adds a static source contract that rejects recurrence;
- runs rustfmt on an ephemeral host copy rather than mutating frozen source bytes;
- keeps format/check/test/clippy as hard pre-package gates.

No frozen render-core or Metal math changed.


## v0.8.4 Metal compiler compatibility hotfix

Only SDK-less host/bridge/build-contract inputs changed. The v0.6 render core and generated Metal shader source remain unchanged. Modern macOS SDKs use safe/precise Metal compile options instead of deprecated `fastMathEnabled`.

## v0.8.5 MFR/cfg compiler hotfix

The fifth Mac run reached Rust host compilation and exposed a cfg propagation mismatch: the PiPL advertised MFR but the destination Rust crate had not activated `threaded_rendering`, causing the generated trait to require mutable global state. v0.8.5 explicitly registers all macro cfg names and pins `threaded_rendering`, `smart_render`, and `gpu_render` in the host build script. This is host scaffolding only; the frozen render core and Metal shader/math are unchanged.

## v0.8.6 deterministic cfg propagation hotfix

The v0.8.5 real Mac compile still selected the non-threaded `after-effects` macro branch. v0.8.6 no longer relies solely on build-script cfg output: the one-click runner explicitly passes `--cfg threaded_rendering --cfg smart_render --cfg gpu_render --cfg catch_panics` through `RUSTFLAGS` for fmt/check/test/clippy/build, and the host source contains compile-time guards that reject any missing required cfg. Render core and Metal math remain unchanged.
