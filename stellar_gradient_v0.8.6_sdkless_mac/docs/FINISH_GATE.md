# Stellar Gradient completion gate

Date: 2026-09-28. Baseline: `7bdbc611c52f0ace7a465bd555b534e40b4405f3` (v0.9.5). Work branch: `fix/stellar-release-gate`. Main and the separate Hot Loader branches are not changed by this work.

Governing rules: https://github.com/ios3kov/FSTR-Line/blob/main/DEVELOPMENT_RULES.md, reviewed blob `a1760fde8763f789b50b91c20407938b4fcaea4a`.

## Acceptance fixed before implementation

Native After Effects effect. Current candidate target: macOS Apple Silicon, CPU 8/16/32 bpc and Metal. No Windows/Intel runtime verification is claimed. Full Cosmic visual/preset equivalence requires controlled reference output, not source inspection alone.

- Preserve parameter disk IDs and the current rendering architecture.
- Only Palette expanded by default.
- Validate initialization, control tree, presets and all non-color settings against the reference. Do not assume a five-color comparison establishes full-preset parity.
- Validate CPU/Metal, ROI, downsampling, alpha/HDR, MFR, undo/save/reopen, restart and installation identity before release.
- No delivery until all required real-AE checks have current evidence for the exact artifact.

## Baseline findings

- STATUS.md records AE initialization failure 25::3 in v0.9.4. The v0.9.5 fix is implemented; real-AE confirmation for that exact build has not been established in this review.
- Root README still describes v0.8.6 and is not an authoritative current status.
- Existing CI compiles Metal and tests the CPU bridge; this is not a Metal pixel comparison or an After Effects session.
- Existing bundle job is independent of quality checks and has no explicit final ZIP hash/commit sidecar.
- The current palette callback changes five colors only. Earlier STATUS.md entries contradict one another; full visual parity remains unverified. Do not silently reset unrelated parameters based on those historical statements.

## Stage 1: reproducible baseline and CI evidence

Implemented: branch/PR checks, read-only workflow permissions, timeouts, source snapshot tied to commit and SHA-256, persistent test logs, package dependency on all code-side checks, final ZIP SHA-256 and commit sidecars. Render/host source unchanged.

Validation: workflow YAML and dependency contract checked locally. GitHub job results pending; not PASS until observed. Production installation, preferences and user projects untouched.

Research: use GitHub's native job dependencies and immutable artifacts rather than a custom uploader. References: https://docs.github.com/en/actions/tutorials/store-and-share-data and https://docs.github.com/en/actions/reference/workflow-syntax-for-github-actions#jobsjob_idneeds .

## Required remaining gates

| Gate | Status at baseline | Evidence needed |
| --- | --- | --- |
| New branch code regression + macOS package | NOT RUN | Current Actions run and logs |
| Runtime build identity | NOT RUN | About/diagnostic identity bound to signed payload |
| AE initialization 25::3 regression | NOT RUN | Real AE load/apply/render with exact build |
| Original full control/preset/output parity | NOT RUN | Reference and candidate outputs under identical conditions |
| Metal execution + numerical CPU parity | NOT RUN | Executed GPU test, tolerances and difference report |
| AE lifecycle, migration, MFR, color/bit-depth matrix | NOT RUN | Isolated host regression record |
| Real AE performance baseline | NOT RUN | Repeated comparable timing/resource records |
| Final delivery | BLOCKED | All mandatory gates PASS for unchanged candidate |

Historical reports remain historical. A successful code-side CI run must not be described as release approval.
