# SDK-less Host Fixes v0.8

Date: 2026-09-26
Status: COMPLETE

## Fixed

- Enabled `catch_panics` for optimized release builds while keeping `panic = "unwind"`.
- Replaced render-time `gpu_data::<GpuContext>()` ownership reconstruction with a plugin-owned `Box<GpuContext>` allocated once at GPU setup, read immutably during GPU renders, and reclaimed exactly once during GPU setdown.
- Stored the native Metal context pointer as `usize` in the Rust-side GPU context so the render-side state is immutable plain data.
- Added C++ ABI `static_assert`s for all bridge structs and critical offsets.
- Added Rust unit tests for the same sizes, alignments and offsets.
- Pinned public parameter IDs directly on `#[repr(i32)] enum Params` (1..49). Add helpers reject mismatched literal IDs.
- Added static verifier for parameter-ID uniqueness/continuity.
- Added Mac preflight gates: `cargo fmt --all -- --check`, `cargo check`, `cargo test`, `cargo clippy -D warnings` before release packaging. Formatting/checks run on an ephemeral host copy so frozen source bytes are not rewritten.
- Cleared external `RUSTFLAGS` / `CARGO_ENCODED_RUSTFLAGS` before the release candidate build.
- Hardened local signing with hardened-runtime option.

## Verification completed in current environment

- SDK-less static host contract: PASS
- Frozen core contract: PASS
- Metal source contract: PASS
- AE SmartFX/MFR source contract: PASS
- C++ bridge strict `-Werror`: PASS
- C++ bridge ASan/UBSan: PASS
- CPU bridge parity: `max_err = 0`

## Mac-only verification still pending

The following cannot be executed in the current Linux environment and are enforced by `FIRST_MAC_BUILD.command` on the user's Apple Silicon Mac:

- Rust `cargo fmt --check`
- Rust `cargo check --release --target aarch64-apple-darwin`
- Rust `cargo test --release --target aarch64-apple-darwin`
- Rust `cargo clippy --release --target aarch64-apple-darwin -- -D warnings`
- Objective-C++/Metal compilation through Xcode clang
- bundle load in After Effects

No Adobe After Effects SDK is required.

## v0.8.1 Mac bootstrap correction

The first real Mac run found that Cargo/rustc can exist without rustup. The launcher now treats Rust bootstrap as a capability check rather than a Cargo-presence check. Rust 1.85+ plus rustfmt/clippy is accepted directly; otherwise stable Rust is installed user-locally via rustup.

## v0.8.2 Mac sanitizer correction

The second real Mac run exposed a preflight-only portability issue: Apple's ASan runtime rejects LeakSanitizer `detect_leaks=1`. The sanitizer gate now runs with `detect_leaks=0` and explicit ASan/UBSan halt-on-error settings. Address and undefined-behavior checks stay mandatory; leak/resource lifetime is verified later with macOS-native AE/Metal profiling tools.

## v0.8.3 Rust-host compile correction

The third real Mac run reached the Rust quality gate after bridge sanitizer PASS. Rust correctly rejected leading-dot float literals such as `.13` and `.01`. v0.8.3:

- rewrites all Rust fractional literals to canonical `0.xx` syntax;
- adds a static verifier for leading-dot float recurrence;
- formats/builds an ephemeral `.sdkless-build` copy while preserving the SHA-frozen canonical source;
- keeps fmt/check/test/clippy mandatory before plugin packaging.

The frozen C++ render core and Metal math are unchanged.


## v0.8.4 Metal compile-option correction

The fourth Mac preflight reached native Metal bridge compilation and Xcode rejected deprecated `fastMathEnabled` under `-Werror`. The bridge now sets `MTLMathModeSafe` and `MTLMathFloatingPointFunctionsPrecise` on macOS 15+ and uses a guarded legacy fallback on older SDK/runtime combinations. This preserves the exact no-fast-math quality policy.

## v0.8.5 Rust cfg/MFR correction

The v0.8.4 Mac run reached real Rust compilation. The `after-effects` macro generated the non-MFR trait shape because the destination crate did not receive `threaded_rendering`, despite the PiPL MFR flag. v0.8.5 now:

- emits `cargo:rustc-check-cfg` for every cfg name expanded by the macro;
- emits `cargo:rustc-cfg=threaded_rendering`, `smart_render`, and `gpu_render` directly from this build script;
- removes unnecessary mutable bindings in GPU setup/setdown.

This preserves shared immutable global state for MFR and keeps `clippy -D warnings` clean without changing the renderer.
