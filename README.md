# Stellar Gradient — After Effects native effect

## Current development source: v0.9.9 (not a release)

The active standalone effect lives in `stellar_gradient_v0.8.6_sdkless_mac/`; the directory name is historical. Work on completion is isolated in `fix/stellar-release-gate`. The separate Hot Loader experiments are not part of this candidate.

v0.9.9 corrects Metal row-pitch validation and checks buffer capacities and geometry before encoding. The old-wrapper reproducer fails as expected and the corrected entry passes; all eight Mac CI jobs pass. These entry tests use Objective-C metadata doubles, not real GPU rendering. CPU/shader math, defaults, saved parameter IDs and goldens are unchanged from v0.9.8, preserving the base-palette and final-stage Grain corrections. See the [current verified status](stellar_gradient_v0.8.6_sdkless_mac/docs/DEVELOPMENT_STATUS_2026-09-28.md) and [Metal validation contract](stellar_gradient_v0.8.6_sdkless_mac/docs/METAL_BUFFER_VALIDATION.md) for exact identity and limitations. New AE/GPU execution remains NOT RUN. Turbulence, Softness, full presets and Cosmic equivalence remain open. Package version, PiPL version and About now come from Cargo's package version. About includes the generated Build ID; the signed package includes commit, source state, source hashes and toolchain metadata. CI requires clean canonical source, a committed dependency lock and passing code-side checks before packaging.

**Code compilation is not release approval.** Actual After Effects initialization, original-preset/output parity, GPU execution parity, lifecycle/migration and host profiling are still mandatory. See [completion gate](stellar_gradient_v0.8.6_sdkless_mac/docs/FINISH_GATE.md) and [build identity](stellar_gradient_v0.8.6_sdkless_mac/docs/BUILD_IDENTITY.md).

## Build / validation

GitHub `Mac CI` validates core strict/sanitizer tests, native bridge, Rust host and clean ARM64 packaging. Its artifact is an internal candidate until the AE gate is complete. It contains the final ZIP SHA-256, commit and signed-payload manifest.

Requirements: Git, Python 3, modern Rust with edition 2024 support, and Apple Xcode command-line tools for macOS builds. Dependencies are locked in `sdkless/Cargo.lock`; release Cargo commands must use `--locked`.

The legacy `FIRST_MAC_BUILD.command` includes installation and environment changes and is **not the approved release path**. It has not passed the new clean-install safety gate. Do not use it as an automatic user-test handoff.

Historical reports in STATUS.md remain historical and do not certify this candidate. No full Cosmic equivalence, production performance advantage, Windows compatibility or Intel runtime compatibility is claimed.
