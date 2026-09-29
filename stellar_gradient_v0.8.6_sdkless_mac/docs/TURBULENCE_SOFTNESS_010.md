# v0.10.0 — Turbulence + Softness correction

Date: 2026-09-29. Governing rules re-read at blob `701a8c1ae3acb4dbfe1d7eda94acbf8095b88608`.
This is a development checkpoint, not release approval. Main and Hot Loader remain out of scope.

## Baseline / evidence

The retained real-AE isolation run `Stellar-Isolate-1790622394083-275372080` contains 22 paired 512×288 RGB8 captures from Stellar v0.9.6 and registered Cosmic 1.0x1. It established that the old Stellar Turbulence was far stronger/finer than Cosmic and that Softness changed Cosmic even when isolated from Grain/Glow/Diffusion. The reference contains a visible red diagonal overlay; scalar comparisons exclude that overlay plus a three-pixel safety border without reconstructing concealed pixels.

The observable Cosmic parameter tree places **Turbulence inside Depth**, then closes Turbulence, exposes **Softness**, then closes Depth. Persistent Stellar IDs are unchanged, but setup order/nesting now follows that observed hierarchy.

## Turbulence

The supplied reference binary's observable CUDA PTX exposes a deterministic 4-D improved-Perlin displacement model. The clean-room implementation reproduces the observable numerical contract on CPU and Metal: two independent three-octave fields displace X/Y in layer pixel coordinates; Size maps to 32 pixels per UI unit; UI Amount maps to pixel displacement; Evolution is periodic in one turn. The second field uses its independently observed coordinate/evolution offsets.

A dedicated test pins independent field samples and +360° periodicity. No proprietary binary/shader is included in the repository.

## Softness

Softness is no longer treated as fBm octave/persistence control. Reference captures and the observable control hierarchy establish it as a separate post-color smoothing stage. v0.10.0 applies three separable box-filter pairs to unpremultiplied working-space RGB, then reapplies source alpha; negative and extended-range RGB are preserved. UI 40 maps to a pass radius of 13. The same stage order is implemented on CPU and Metal.

The exact host-side dispatch formula is not directly visible in the PTX, so this part is supported by output comparison plus the existing observable box-blur pipeline, not claimed as recovered source code.

## Local comparison before Mac/AE build

Using the same RGB8/sRGB projection previously validated against the installed Stellar base, and excluding only the visible reference overlay region:

- Turbulence Size 3 vs Cosmic: MAE **0.0751** code values, RMSE **0.3234**, max **4**.
- Turbulence Size 6 vs Cosmic: MAE **0.0739**, RMSE **0.3200**, max **4**.
- Turbulence + Softness 40 vs Cosmic: MAE **0.7031**, RMSE **2.5194**, max **29**.
- Existing Depth-only residual remains larger (MAE **2.3760**) and is not hidden by this stage.

These are local CPU/output-projection measurements, **not real AE or executed Metal evidence**. They do not certify full presets, HDR, alpha through the host, or GPU dispatch.

A CPU lattice-hash cache preserves the compared F32 outputs bit-for-bit while avoiding repeated permutation walks. Short warmed sandbox timing remains mixed: base/full cases are near the prior range, but the isolated 1280×720 procedural case is still substantially slower than v0.9.9 because the corrected reference uses two 4-D fields × three octaves. This is recorded as a performance risk, not hidden or called a speedup; final performance acceptance requires real AE/Metal profiling.

## Acceptance for this commit

Regression Level 1 requires strict/ASan+UBSan/TSan core suites, dedicated Turbulence/Softness contracts, unchanged base signature, reviewed heavy/diffusion golden updates without tolerance relaxation, tile/ROI/stride/alpha/HDR contracts, Rust/static checks, actual MSL compilation, bridge parity/MFR, and ARM64 packaging. Any FAIL blocks handoff.

Before user acceptance, Regression Level 2 still requires the exact built artifact in real After Effects, loaded Build ID, CPU/GPU/bit-depth/output checks, restart/save-reopen/Undo/MFR/noninteractive paths and profiling as applicable.
