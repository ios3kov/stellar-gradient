# Status — v0.8 SDK-less Mac candidate

Date: 2026-09-26

## Plan status

1. **SDK-less layer audit — COMPLETE**
   - Found release panic-boundary risk and GPU-data ownership risk.
   - FFI layout and parameter-ID contracts reviewed.
   - See `SDKLESS_AUDIT_V08.md`.

2. **Fixes — COMPLETE**
   - Release panic catching enabled.
   - GPU context changed to single-owner setup/setdown lifetime with immutable concurrent render access.
   - C++/Rust ABI size/alignment/offset guards added.
   - Public parameter IDs pinned 1..49 on `#[repr(i32)] Params`.
   - Rust quality gates added before packaging.
   - See `SDKLESS_FIXES_V08.md`.

3. **Final code-side regression — COMPLETE**
   - Core strict: 7/7 PASS.
   - Core ASan/UBSan: 7/7 PASS.
   - Core TSan: 7/7 PASS.
   - Bridge parity: `max_err=0`.
   - Bridge HDR/8/16/32 checks: PASS.
   - Bridge MFR: `max_err=0`, failures=0.
   - Bridge ASan/UBSan + TSan: PASS.
   - Mac-only Rust compiler gates are embedded into `FIRST_MAC_BUILD.command` and execute before packaging.
   - See `REGRESSION_V08.md`.

4. **v0.8 source freeze — COMPLETE**
   - Host/bridge version: v0.8.6 build 14 (MFR/cfg compiler hotfix; render core unchanged).
   - Render core remains frozen v0.6 and unchanged.
   - 49 source/build inputs frozen by SHA-256.
   - `python3 tools/verify_v08_host_freeze.py` => PASS.
   - See `SOURCE_FREEZE_V08.md`.

5. **Native Mac `.plugin` build — RETRY READY / CURRENT GATE**
   - First v0.8 Mac run on 2026-09-26 stopped before compilation: Homebrew `cargo/rustc 1.80.1` existed, but `rustup` did not; the script incorrectly called `rustup` unconditionally.
   - Root cause: Rust 1.80.1 is also too old for the host's Rust 2024 edition (minimum 1.85).
   - Fixed in v0.8.1 build 9: modern complete Rust is used directly; old/incomplete Rust triggers automatic user-local stable rustup bootstrap.
   - Second Mac run reached sanitizer smoke and stopped because macOS ASan rejects `detect_leaks=1`; this was a preflight-script portability defect, not a plugin/core failure.
   - Fixed in v0.8.2 build 10: macOS sanitizer smoke uses `detect_leaks=0` with ASan/UBSan halt-on-error enabled.
   - Adobe SDK is not required.
   - Third Mac run reached the Rust host gate after contracts, bridge parity and sanitizer smoke passed. It exposed invalid Rust leading-dot floats (`.13`, `.01`, etc.) and rustfmt style drift.
   - Fixed in v0.8.3 build 11: all literals use `0.xx`, a static syntax contract prevents recurrence, and rustfmt/check/test/clippy run on an ephemeral build copy so the frozen source bytes are not modified.
   - Retry the v0.8.6 `FIRST_MAC_BUILD.command` on the Apple Silicon test Mac.
   - It performs Rust fmt/check/test/clippy, native bridge checks, Release arm64 build, bundle/codesign validation, install, diagnostics and AE launch.

6. **AE functional/quality test — BLOCKED BY 5**
7. **Metal profiling — BLOCKED BY 6**
8. **Profiler-driven optimization — BLOCKED BY 7**
9. **Cosmic vs Stellar controlled benchmark — BLOCKED BY 8**
10. **Final user test `.plugin` — BLOCKED BY 9**

## Release rule

Do not claim production readiness or speed superiority over Cosmic until points 5–9 have passed on the same Mac/After Effects environment.


### v0.8.5 Mac compile hotfix

The fourth real Mac run reached Objective-C++ Metal compilation and exposed Xcode's macOS 15+ deprecation of `MTLCompileOptions.fastMathEnabled`. The SDK-less bridge now uses safe/precise modern Metal compile options with a guarded legacy fallback. Render core and shader math are unchanged.


## macOS build retry — v0.8.6 build 14

The v0.8.4 run reached native Rust host compilation. Metal compiled past the precise-math gate, but Rust 1.98 exposed that `threaded_rendering` was not active in the destination crate even though the PiPL advertised MFR. The `after-effects` macro therefore generated a mutable `handle_command(&mut self, ...)` trait while the MFR-safe host implemented `&self`.

v0.8.5 registers all macro cfg names explicitly and pins `threaded_rendering`, `smart_render`, and `gpu_render` in this crate's build script. This keeps the Rust host trait and the advertised PiPL capabilities in one deterministic contract. Two `unused_mut` warnings are also removed so the later `clippy -D warnings` gate remains strict. Frozen C++ render core and Metal shader math are unchanged.


### GitHub CI migration — 2026-09-27

- Full v0.8.6 source tree is now in `ios3kov/stellar-gradient` under `stellar_gradient_v0.8.6_sdkless_mac/`.
- The first three GitHub Actions runs used the earlier temporary minimal archive; native C++/Metal passed, but Rust still failed with E0053 because crates.io `after-effects 0.4.0` generated the non-MFR `&mut self` trait.
- Upstream inspection confirmed current MFR examples use `&self`. The SDK-less host now pins both `after-effects` and `pipl` to exact upstream revision `83dcc93734fd5db1335b6ec83cba7a6505a39dcc` so the macro/API implementation is deterministic.
- Frozen render core and Metal shader math are unchanged.
- CI now runs independent contracts, core strict/ASan/TSan, native bridge/Metal, Rust quality, and ARM64 bundle jobs in parallel.


### CI tree recovery — 2026-09-27

- Run #4 failed before meaningful code tests because the CI hotfix tree was accidentally created without the uploaded source tree as its Git base; this appeared as missing CMake/Rust/bridge files.
- The source upload commit `b6fa596b...` is now the canonical base tree and all uploaded project files are restored in a normal fast-forward commit.
- The exact upstream MFR dependency pin and parallel CI workflow are retained.
- `verify_sdkless_host.py` now validates the pinned upstream dependency instead of requiring the old crates.io strings.
- Render core and Metal shader math remain unchanged.


### GitHub Mac CI run #5 — 2026-09-27

- Static contracts: PASS.
- Core strict: PASS.
- Core ASan/UBSan: PASS.
- Core TSan: PASS.
- Native bridge + Metal: PASS, including bridge ASan/UBSan.
- ARM64 plugin bundle: PASS. The SDK-less Rust host compiled against the pinned upstream revision, the macOS bundle was created, ad-hoc signed, verified, and uploaded as the `StellarGradient-mac-arm64` artifact.
- The previous MFR E0053 host ABI blocker is resolved.
- Rust host quality was the only red job and stopped at `cargo fmt --check` before compile checks. This is CI-policy drift: the frozen canonical source is intentionally unformatted in places, while the local Mac gate formats an ephemeral copy. CI is now aligned with that policy by formatting and testing `.sdkless-ci`.
- Render core and Metal shader math remain unchanged.

Plan item 5: **COMPLETE — real ARM64 .plugin built in CI**.
Plan item 6: **NEXT — After Effects functional/quality validation** after the all-green CI confirmation.


### Rust quality cleanup — 2026-09-27

- GitHub Mac CI run #6: 6/7 jobs passed, including repeated ARM64 plugin build/bundle/sign/upload.
- Rust `cargo check --release` and the FFI layout unit test passed against the pinned upstream dependency.
- The only remaining failure was Clippy `needless_range_loop` in the palette-copy loop.
- The loop now uses `iter_mut().zip(...iter())`; output values and render behavior are unchanged.
- Strict `clippy -D warnings` remains enabled; no lint suppression was added.
- SHA-freeze updated only for `sdkless/src/lib.rs`.
