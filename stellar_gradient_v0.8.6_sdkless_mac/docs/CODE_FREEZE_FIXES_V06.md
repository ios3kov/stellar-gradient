# Code-freeze fixes v0.6

Date: 2026-09-26
Plan point 2/10: COMPLETE.

## Implemented

- Split persistent parameter disk IDs from `ParamIndex`; v0.5 numeric IDs 1..49 are frozen for project compatibility.
- Switched AE color parameter reads to `PF_ColorParamSuite1::PF_GetFloatingPointColorFromColorDef()` so colors stay in the effect working space and preserve HDR over-range / under-range values.
- Color sanitization now rejects NaN/Inf while preserving finite HDR values (bounded only to ±65504 for safe Preview FP16 representation).
- Added Metal `supports32BitFloatFiltering` capability tracking. Auto/Final only advertise GPU rendering when FP32 filtering is supported; Preview can still use FP16.
- GPU device probe/initialization failures now cleanly reject GPU instead of aborting the effect, leaving CPU SmartRender available.
- Empty SmartFX output is an explicit successful no-op.
- Added source-contract verification for all code-freeze invariants.
- Extended hardening tests with HDR palette over/under-range and 1×1 extreme Glow/Diffusion.

## Immediate validation

- metadata: PASS
- Metal source contract: PASS
- AE SmartFX contract: PASS
- code-freeze source contract: PASS
- Release `-Werror`: PASS
- core tests: 7/7 PASS

No CPU golden signatures changed.
