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

## CI

GitHub Actions builds and tests the SDK-less macOS plug-in without the Adobe SDK. Successful runs upload `StellarGradient.plugin` as an artifact.

## Mac build — no Adobe SDK

Double-click:

`FIRST_MAC_BUILD.command`

See `docs/STATUS.md`, `docs/MAC_GATE_STATUS.md`, `docs/REGRESSION_V08.md`, and `docs/MAC_TEST.md`.
