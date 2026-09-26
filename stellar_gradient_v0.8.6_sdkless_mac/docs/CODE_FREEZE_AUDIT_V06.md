# Code-freeze audit v0.6

Date: 2026-09-26
Scope: final source-only audit before the first native macOS / After Effects build.

## Status

Point 1 (audit): COMPLETE.

## Findings that must be fixed before freeze

1. **Parameter disk IDs are coupled to UI/index order.**
   The `PF_ADD_*` calls currently use `ParamIndex` values as persistent parameter IDs. Adobe persists parameters by ID, so adding/reordering controls later could break old projects. Fix: introduce explicit immutable disk IDs while keeping the current numeric values for backward compatibility.

2. **Color parameters are read through the legacy 8-bit `PF_Pixel` field.**
   In a 32-bpc/HDR project this loses over-range/under-range floating-point color values. Adobe provides `PF_ColorParamSuite1::PF_GetFloatingPointColorFromColorDef()` specifically to retrieve the color in the effect working space. Fix: use the floating-point suite and preserve finite HDR colors.

3. **Metal FP32 filtering capability is not checked.**
   Auto/Final use `RGBA32Float` mip textures plus linear/trilinear sampling. Metal exposes `supports32BitFloatFiltering`; GPU rendering must be rejected for Auto/Final on devices that cannot filter 32-bit float textures. Preview may still use FP16.

4. **GPU setup can propagate `GetDeviceInfo` failure instead of cleanly rejecting the device.**
   Device setup is the intended fallback gate. Fix: leave GPU support flags cleared and return success when device probing/Metal pipeline creation cannot establish a supported GPU path; CPU SmartRender remains available.

5. **Empty SmartFX output has no explicit no-op guard.**
   Empty/fully clipped requests can reach render code whose work-rect validation treats zero size as an internal error. Fix: make empty output a valid zero-work render.

## Verified / no source change required

- Release C++ build explicitly disables fast-math / unsafe-math.
- Metal runtime compilation sets `fastMathEnabled = NO`.
- Glow/Diffusion/Grain/Turbulence are already skipped by the render plan when inactive; Depth skips `pow()` when Bulge is zero.
- CPU F32 preserves HDR RGB > 1.0 and premultiplied base output.
- MFR CPU workspace is thread-local and the stress test is deterministic.
- Metal device-owned pipeline state is created once per GPU setup and released in GPU device setdown.
- Per-frame Metal textures remain transient by design for the first host build; pooling is deferred until Instruments proves it is material, to avoid adding synchronization/VRAM complexity before profiling.

## Host-only gates (cannot be proven in this environment)

- Actual CPU↔Metal numerical parity on Apple Silicon.
- Metal mip/filter behavior on the user's GPU.
- AE color-management behavior across real Linear/HDR projects.
- Host resource lifetime under real MFR GPU scheduling.
