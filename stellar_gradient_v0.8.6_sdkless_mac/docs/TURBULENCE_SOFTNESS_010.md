# v0.10.0 — Turbulence + Softness correction

Date: 2026-09-29. Original correction checkpoint used the then-current rules blob `701a8c1ae3acb4dbfe1d7eda94acbf8095b88608`. Continuation on 2026-10-01 first used AE Development Rules v3.1.1; later the project adopted **v4.0.0** commit `58d14aa12375757f4e52396d951f1557ae4f453c`, `AI_ENTRYPOINT.md` blob `29cb44eb7f2a4e0f7c120a97ae434dbb7e58174e`. Cosmic is an explicit whole-product parity target, so the v4 Reference Audit overlay applies; current specification is `REFERENCE_SPECIFICATION_COSMIC_V4.md` with status PARTIAL.
This remains a Validation checkpoint, not release approval. Main and Hot Loader remain out of scope.

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

The original lattice-hash cache remains, and commit `9f3e73bc92534941db1106f521786d8c3c792347` additionally precomputes X/Y lattice index, fractional coordinate and fade for the regular render grid. Metal is unchanged. A direct renderer-versus-uncached-`cosmic_fbm4` regression protects the field contract; strict core tests are 11/11 PASS and ASan+UBSan/TSan are PASS.

Performance is measured against the correct v0.10.0 behavior baseline `16cfa398e7b0f1dd2701e3c8c40ba42e8bb42857` on the same `macos-15` runner, with alternating baseline/current execution and 11 paired samples per measurement. Three completed paired measurements (push attempt 1, PR attempt 1, push attempt 2) all measured the 1280×720 procedural case faster: paired ratios 0.969, 0.915 and 0.837. Their median is 0.915; normalized by the unchanged 1280×720 base-path control in each measurement, the median is about 0.940. Hosted-runner variance is visibly material, so this is only evidence of a repeatable Level-1 CPU improvement direction, not a production or real-AE speed claim. Final performance acceptance still requires real After Effects/Metal profiling.

## Acceptance for this commit

Regression Level 1 requires strict/ASan+UBSan/TSan core suites, dedicated Turbulence/Softness contracts, unchanged base signature, reviewed heavy/diffusion golden updates without tolerance relaxation, tile/ROI/stride/alpha/HDR contracts, Rust/static checks, actual MSL compilation, bridge parity/MFR, and ARM64 packaging. Any FAIL blocks handoff.

Before user acceptance, Regression Level 2 still requires the exact built artifact in real After Effects, loaded Build ID, CPU/GPU/bit-depth/output checks, restart/save-reopen/Undo/MFR/noninteractive paths and profiling as applicable.
