# Stellar Gradient — After Effects native effect

## Current development source: v0.10.0 (validation candidate, not a release)

The active standalone effect lives in `stellar_gradient_v0.8.6_sdkless_mac/`; the directory name is historical. Completion work remains isolated in `fix/stellar-release-gate`. The separate Hot Loader experiments are not part of this candidate.

v0.10.0 restores the observed Cosmic-style 4-D Turbulence contract on CPU and Metal and implements Softness as an independent post-color smoothing stage. The latest native CPU performance change is commit `9f3e73bc92534941db1106f521786d8c3c792347`: it precomputes regular-grid Turbulence axis state without changing Metal. A direct cached-renderer versus uncached `cosmic_fbm4` regression was added to guard output parity.

Exact code-side validation for `9f3e73b`: Mac CI run `36911997718` passed all 8 jobs, including strict/ASan+UBSan/TSan core suites, native bridge + actual Metal source compilation, Rust checks and ARM64 packaging. The exact ARM64 artifact is `11187002476`; its Build ID is `sg-0.10.0-9f3e73bc9253-clean-bc5efe6bf48a-aarch64-apple-darwin-36911997718.1`. The packaged plugin ZIP SHA-256 is `4195641fe2b2a79e528cd1a80b3a10f33b278b3789fface7d71a80370a0e1c30`.

Synthetic CPU performance is compared against the correct v0.10.0 behavior baseline `16cfa398e7b0f1dd2701e3c8c40ba42e8bb42857`, not the visually incorrect v0.9.9 path. Three 11-sample paired measurements on `macos-15` all measured the 1280×720 procedural case faster; the median raw paired ratio is 0.915, and the median ratio normalized to the unchanged base-path control is about 0.940. Hosted-runner variability is still material, so this is Level-1 evidence only, not a claim about real After Effects or Metal performance.

Later commits `cc21f1e` and `59448db` change only performance measurement tooling. See the [current verified status](stellar_gradient_v0.8.6_sdkless_mac/docs/DEVELOPMENT_STATUS_2026-09-28.md), [Turbulence/Softness checkpoint](stellar_gradient_v0.8.6_sdkless_mac/docs/TURBULENCE_SOFTNESS_010.md), [performance evidence](stellar_gradient_v0.8.6_sdkless_mac/docs/evidence/CPU_TURBULENCE_PERF_010_CI.json) and [completion gate](stellar_gradient_v0.8.6_sdkless_mac/docs/FINISH_GATE.md).

**Code-side validation is not release approval.** Regression Level 2 still requires this exact built artifact in real After Effects: loaded runtime Build ID, actual CPU/GPU execution and numerical parity, 8/16/32 bpc and alpha/HDR checks, restart/Undo/save-reopen/migration, MFR/render queue paths and real-host profiling.

## Build / validation

GitHub `Mac CI` validates core strict/sanitizer tests, native bridge, Rust host and clean ARM64 packaging. Its artifact remains an internal validation candidate until the AE gate is complete.

Requirements: Git, Python 3, modern Rust with edition 2024 support, and Apple Xcode command-line tools for macOS builds. Dependencies are locked in `sdkless/Cargo.lock`; release Cargo commands must use `--locked`.

The legacy `FIRST_MAC_BUILD.command` includes installation and environment changes and is **not the approved release path**. Do not use it as an automatic user-test handoff.

Historical reports remain historical and do not certify the current candidate. No full release approval, real-host performance advantage, Windows compatibility or Intel runtime compatibility is claimed.
