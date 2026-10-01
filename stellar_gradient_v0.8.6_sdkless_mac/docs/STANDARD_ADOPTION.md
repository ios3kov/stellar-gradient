# AE Development Standard Baseline — Stellar Gradient

- Standard repository: https://github.com/ios3kov/AE-Development-Rules
- Standard version: **4.0.0**
- Standard commit: `58d14aa12375757f4e52396d951f1557ae4f453c`
- AI Smart Entry blob: `29cb44eb7f2a4e0f7c120a97ae434dbb7e58174e`
- Adopted on: **2026-10-01**
- Project milestone: **Stellar Gradient v0.10 validation**
- Reference Audit status: **PARTIAL**
- Reference Specification: `docs/REFERENCE_SPECIFICATION_COSMIC_V4.md`
- Stage 0 status: **EXISTING PRODUCT CONTRACT**
- Current Risk Profile: **Critical** — native render path / threading / CPU-GPU / performance-sensitive effect
- Current Delivery Gate: **Validation**
- Rules manifest schema: **2**
- Technology Lifecycle decisions: native AE effect; UXP/CEP not in scope
- Adoption owner: project

## Project-specific additions

- Cosmic is an explicit whole-product behavior/render reference for the common effect surface.
- Stellar remains an independent implementation; proprietary reference code/assets are not redistributed.
- Stellar-only `Render Engine` and `Quality` controls are intentional additions.
- Exact Cosmic Grain random seed/pattern is not a parity requirement; envelope/stage behavior is.
- Historical evidence keeps its original identity/status and is never relabelled as current-artifact evidence.

## Approved deviations

None.

## Upgrade history

| Date | From | To | Standard commit | Notes |
| --- | --- | --- | --- | --- |
| 2026-10-01 | 3.1.1 | 4.0.0 | `58d14aa12375757f4e52396d951f1557ae4f453c` | v4 Reference Audit triggered because Cosmic is the explicit parity target; created Reference Specification and Coverage Map. No native source/artifact change. |

## Current candidate boundary

Latest native-code commit: `9f3e73bc92534941db1106f521786d8c3c792347`.

The current documentation head may be newer. Documentation-only commits do not
relabel or replace the exact native validation artifact built from the native-code
commit above.

Release remains **NOT APPROVED** until the applicable real-AE Validation / later
Release gates are completed on an unchanged identified candidate.
