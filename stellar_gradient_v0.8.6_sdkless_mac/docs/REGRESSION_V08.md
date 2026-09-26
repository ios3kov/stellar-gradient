# Regression v0.8 — SDK-less Host Candidate

Date: 2026-09-26
Status: PASS for all checks executable in the current environment.

## Frozen core

- strict Release: 7/7 PASS
- ASan/UBSan: 7/7 PASS
- TSan: 7/7 PASS
- golden/reference tests: PASS
- SmartFX tile consistency: PASS
- hardening/extreme cases: PASS
- core MFR stress: PASS

## SDK-less native bridge

- source contracts: PASS
- strict C++ `-Werror`: PASS
- CPU bridge/reference parity: `max_err = 0`
- 32-bpc HDR bridge: PASS, measured RGB peak = 2.0 (not clipped)
- 8-bpc clipping: PASS
- 16-bpc clipping: PASS
- concurrent bridge MFR test: PASS, `max_err = 0`, failures = 0
- bridge ASan/UBSan: PASS
- bridge TSan: PASS

## Rust host

Release safety and build gates are encoded in source/build scripts:

- `panic = "unwind"`
- release `cfg(catch_panics)` enabled
- C/Rust ABI size/offset tests
- stable public IDs 1..49
- `cargo fmt --all -- --check` (on ephemeral build copy)
- `cargo check --release`
- `cargo test --release`
- `cargo clippy --release -- -D warnings`

Those Rust/macOS compiler gates require the Apple Silicon Mac build environment and therefore remain the first actions inside `FIRST_MAC_BUILD.command` before a plugin is packaged or installed.

## Result

No render-core quality regression was introduced by the SDK-less host hardening. The CPU bridge remains bit-for-bit identical to the frozen reference for the parity scene (`max_err = 0`).

## Real Mac preflight observations

- Rust bootstrap: PASS, active rustc 1.98.1 / cargo 1.98.1.
- Frozen/source/Metal/AE/SDK-less contracts: PASS.
- Native bridge parity on Apple Silicon: `max_err=0`.
- v0.8.1 sanitizer command itself was not portable because `detect_leaks=1` is unsupported by Apple's ASan. v0.8.2 corrects the option without changing the sanitizer-instrumented bridge binary or render math.

- Third Mac preflight: stages 1–3 PASS; Rust gate reached real compiler. Invalid host-only leading-dot float syntax was reproduced and fixed in v0.8.3; a static syntax contract now prevents recurrence.

- Fifth Mac preflight: stages 1–3 PASS, modern Metal compile option PASS; Rust compiler reached and exposed cfg/MFR trait mismatch. v0.8.5 deterministically pins/registers macro cfgs and removes strict-clippy warnings; next Mac run is the native validation gate.
