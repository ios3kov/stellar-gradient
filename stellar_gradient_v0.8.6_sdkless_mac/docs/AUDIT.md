# v0.4 audit / refactor / debug report

## Correctness fixed

- Fixed AE parameter-setup compile blocker (`PF_ADD_*` macro context).
- Removed invalid `PIX_INDEPENDENT` metadata for neighbor-dependent Glow/Diffusion.
- Added `REVEALS_ZERO_ALPHA` consistently.
- Fixed SmartFX tile-space procedural coordinates and Diffusion center.
- Fixed duplicated SmartFX halo/over-render logic.
- Added mip alignment for tile-stable mip phase.
- Expanded and locked CPU/Metal shared GPU parameter ABI (344 bytes).
- Release tests no longer depend on disabled `assert()`.
- Added CPU F32 golden signatures so visual math changes cannot silently pass as "optimization".
- Added `NON_PARAM_VARY` for time-varying animated Grain and dynamic flags to clear it when Grain animation is inactive, restoring cache efficiency without correctness bugs.

## Performance refactor

- Base-only Metal path is one direct compute pass.
- Base + Glow seed/threshold are fused in one compute pass.
- `sin/cos/log2`, bounds normalization and other frame constants moved out of per-pixel Metal work.
- Depth `pow()` is skipped when Bulge is zero.
- Metal parameter buffer allocation removed; small constants use `setBytes`.
- Mip chains stop at the highest required LOD.
- Auto/Final use FP32 intermediates; Preview alone may use FP16.
- CPU reuses a persistent worker pool and thread-local mip storage.
- No explicit `waitUntilCompleted` CPU/GPU stall exists in the normal Metal render path.

## Verification completed here

- Release + strict `-Werror` build.
- AddressSanitizer + UndefinedBehaviorSanitizer.
- Determinism and seamless 360° Phase loop.
- Edge cases / invalid dimensions.
- GPU struct layout.
- Full-frame vs SmartFX-style tiled consistency with Turbulence + Grain + Glow + Diffusion.
- CPU F32 golden quality signatures.
- PiPL/C++ metadata contract.
- Metal performance/quality source contract.
- AE SmartFX/MFR/cache/pixel-format source contract.

## Remaining proof points (Mac only)

1. Objective-C++ build against the installed/current Adobe After Effects SDK.
2. Metal source compile on the target Apple GPU.
3. Load and render in After Effects.
4. CPU vs Metal image-diff.
5. MFR/tile stress in the real host.
6. 1080p/4K Metal profiling + allocation trace.
7. Cosmic comparison on the same Mac/project/settings.
