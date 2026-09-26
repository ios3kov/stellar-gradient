# Stellar Gradient — clean-room AE effect

Independent After Effects effect in the same broad category as Cosmic. It does not contain Cosmic source code, shaders, assets or licensing code.

## Current status — v0.8.6 SDK-less Mac candidate

The render core remains the audited/frozen **v0.6 core**. The After Effects host, C ABI bridge and Metal bridge are now **v0.8.6 build 14** and no longer require the Adobe After Effects SDK.

Completed before the real Mac host run:

- CPU FP32 quality reference locked;
- strict / ASan+UBSan / TSan core regression: 7/7 PASS;
- SDK-less C++ bridge parity: `max_err = 0`;
- bridge 8/16/32-bpc + HDR checks: PASS;
- bridge concurrent MFR test: `max_err = 0`;
- release panic boundary hardened;
- Rust/C ABI layout frozen and tested;
- persistent AE parameter IDs pinned 1..49;
- Metal fast-math disabled for quality paths;
- v0.8 source/build inputs SHA-256 frozen.

## Controls

- 5-color looping palette + presets
- Angle / Cycles / Offset / Phase
- Saturation / Brightness
- Depth: Contrast / Bulge / Rounding
- Turbulence
- Glow: Radius / Falloff / Threshold / Intensity / Soft Clip
- Grain: Amount / Size / Color / Animate
- Optical Diffusion: Blur / Center / Focus / Feather / Invert
- Render Engine: Auto / GPU / CPU
- Quality: Preview / Auto / Final


### Rust bootstrap behavior

- Rust 1.85+ with `rustfmt`/`clippy` already installed: used directly; `rustup` is not required.
- Older/incomplete Rust (including Homebrew Rust 1.80.x): `FIRST_MAC_BUILD.command` automatically installs a user-local stable toolchain via official rustup and continues.
- This does not modify the frozen render core and does not require Adobe SDK.


### macOS sanitizer behavior

- Apple macOS AddressSanitizer does not support LeakSanitizer's `detect_leaks=1` option.
- The Mac preflight therefore runs ASan + UBSan with leak detection explicitly disabled (`detect_leaks=0`) while keeping immediate failure on address/undefined-behavior errors.
- Leak/resource lifetime is checked later in the real AE/Metal profiling stage with macOS-native tools; this change does not weaken address/UB checks.


### v0.8.4 Metal compiler compatibility hotfix

The fourth real Mac preflight reached native Objective-C++/Metal compilation. Xcode's current macOS SDK deprecates `MTLCompileOptions.fastMathEnabled`, and `-Werror` correctly rejected that deprecated API. v0.8.4 uses `mathMode = MTLMathModeSafe` plus `mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise` on macOS 15+ SDK/runtime, with a guarded legacy fallback for older SDKs/runtimes. The quality contract remains unchanged: no unsafe fast math.



### v0.8.6 deterministic MFR/GPU cfg propagation

The v0.8.5 real Mac run proved that `cargo:rustc-cfg` from build.rs was registered for check-cfg but did not activate the destination crate's macro branch. v0.8.6 therefore pins `threaded_rendering`, `smart_render`, `gpu_render`, and `catch_panics` directly in the Cargo rustc invocation via `RUSTFLAGS`. The Rust host also contains compile-time guards that fail immediately if any required cfg is missing. This changes host scaffolding only; frozen render core and Metal shader math are unchanged.

### v0.8.5 Rust MFR/cfg compiler hotfix

The fifth real Mac preflight reached Rust host compilation after Metal compiled successfully. Rust 1.98 showed that the destination crate did not have `threaded_rendering` activated even though the PiPL advertised MFR, so the generated trait expected `&mut self` while the MFR-safe host intentionally implements `&self`. v0.8.5:

- registers all cfg names expanded by the `after-effects` macro for Rust's `check-cfg`;
- explicitly pins `threaded_rendering`, `smart_render`, and `gpu_render` in this crate's build script;
- keeps the PiPL MFR/GPU flags in the same deterministic contract;
- removes two `unused_mut` warnings so `clippy -D warnings` remains strict.

Frozen C++ render core and Metal shader/math are unchanged.

### v0.8.3 Rust-host compile hotfix

The third real Mac preflight reached the Rust host quality gate. Source/bridge/sanitizer stages passed, then Rust compilation exposed leading-dot floating literals such as `.13` and `.01`, which Rust rejects, plus rustfmt-only style drift in `build.rs`. v0.8.3:

- normalizes every Rust fractional literal to the canonical `0.xx` form;
- adds a static preflight contract that rejects any future leading-dot Rust float;
- formats and compiles an ephemeral `.sdkless-build` copy so the SHA-frozen source tree is never mutated by rustfmt;
- keeps `cargo fmt --check`, `cargo check`, `cargo test`, and `cargo clippy -D warnings` mandatory before packaging.

No render-core or Metal math changed.

## Rendering architecture

1. CPU FP32 renderer is the visual reference.
2. Metal base + Glow threshold are fused when Glow is active; base-only renders directly to the AE GPU output.
3. Glow and Diffusion use only the mip levels they actually sample.
4. Stable layer-space procedural coordinates across SmartFX tiles.
5. Aligned internal work halo is cropped to the host-requested output tile.
6. Auto/Final keep FP32 intermediates; only explicit Preview may use FP16.
7. Metal GPU on Mac + CPU fallback. Windows backend comes later from the same core/ROI contract.
8. Rust is only the AE ABI/PiPL/dispatch layer; hot rendering remains C++/Metal.

## Mac build — no Adobe SDK (v0.8.6 deterministic cfg propagation)

Double-click:

`FIRST_MAC_BUILD.command`

Before packaging the plug-in it runs:

- frozen-source verification;
- C++ bridge parity + sanitizer smoke;
- `cargo fmt --check`;
- `cargo check --release`;
- `cargo test --release`;
- `cargo clippy --release -- -D warnings`;
- arm64 Release build;
- bundle verification + local codesign.

Then it installs to:

`~/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/StellarGradient.plugin`

and launches After Effects.

See `docs/STATUS.md`, `docs/MAC_GATE_STATUS.md`, `docs/REGRESSION_V08.md`, and `docs/MAC_TEST.md`.

## Source freezes

- Frozen render core: `docs/SOURCE_FREEZE_V06.md`
- Current SDK-less candidate: `docs/SOURCE_FREEZE_V08.md`
- v0.8 hash manifest: `docs/SOURCE_FREEZE_V08_SHA256.txt`
