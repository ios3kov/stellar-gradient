# Stellar Gradient — verified checkpoint, 2026-09-28

> Current continuation, 2026-10-01: v0.10.0 remains a **Validation candidate, not a release**. The latest native-code commit is `9f3e73bc92534941db1106f521786d8c3c792347`; exact code-side Mac CI run `36911997718` is 8/8 PASS. Later branch commits are measurement, Reference-Audit, diagnostic and documentation work; they do not replace the exact native artifact. The historical v0.9.9 record below is retained as history; current v0.10 evidence is appended at the end.

**v0.9.9 is an internal candidate, not a release.** Work branch:
`fix/stellar-release-gate`, draft PR #1. Main and Hot Loader are unchanged.
The user retains v0.9.6; no new user installation or AE/GPU execution took place.
This document records completed code-side checks without relabelling the binary
to a later documentation commit.

## Exact tested candidate

- Commit: `a147aafbf2b461ebe1890dd8b5a48e9a09976ebd`.
- Clean tree: `b9b0bd4be2692d8429aae18be09c828c9ab7b4ad`.
- Build ID: `sg-0.9.9-a147aafbf2b4-clean-241c9621fb5a-aarch64-apple-darwin-36486084430.1`.
- Push CI: https://github.com/ios3kov/stellar-gradient/actions/runs/36486084430 .
- Internal ARM64 artifact: **10999241777**.
- Artifact ZIP SHA-256: `05f387bf08d2806d39fa30aa9696b3e419ebbd8b2b974b66eb056e25b8cfb599`.
- Plugin ZIP SHA-256: `da9039f3a4fc035c48623e10dab30ba43f5b1c77ccb3ef303af8ae933816d3d4`.

Downloaded artifact checks passed: six payload hashes, compiled Build ID,
plist/JSON version and commit, ARM64 Mach-O header, and all 29 recorded native
source hashes. The binary was not executed, modified or re-signed after download.
CI signature verification is not notarization or evidence of a user AE load.

## Metal buffer defect corrected and reproduced

The old expression `rowbytes % sizeof(float) * 4` allowed 36-byte rows although
the current float4 shaders need a whole 16-byte-pixel pitch. It then truncated
the pitch to 32 bytes. The actual old wrapper at `d7e035d` was compiled on macOS
and called with Objective-C metadata doubles: the reproducer reached a command
buffer request (`result=-3`, one call) instead of rejecting the layout.
The same test against v0.9.9 returns `-1` with zero queue calls.
No actual GPU memory access or user crash is asserted by this experiment.

`MetalValidation.hpp` now checks independent input/output pitches, minimum row
capacity, final logical pixel byte spans, uint32 shader index overflow, signed
geometry arithmetic and valid output crop. Both MTLBuffer lengths are checked
before pipeline access, texture allocation or command-buffer creation. Padding,
unequal input/output pitches and safe translated/empty semantic bounds remain
supported; unused padding after the final visible pixel is not required.

These are requirements of our existing shader representation, not a universal
AE CPU pointer-alignment rule. Arbitrary four-byte-only pitches fail explicitly;
no silent rounding, buffer copies, retries or new CPU fallback were introduced.
CPU rendering, Metal shader math, parameter defaults/IDs, ABI, dependencies,
and all previous golden images/tolerances are unchanged in this stage.
See [design and acceptance](METAL_BUFFER_VALIDATION.md).

## Observed validation

| Check | Status / scope |
| --- | --- |
| Push CI source/static/core/native/Rust/package | **8/8 jobs PASS** for a147aaf |
| Strict, ASan/UBSan and TSan core | **10/10 PASS** each locally and in CI |
| Scalar layout oracle / edge cases | **681,372 checks, zero failures**, no GPU allocation |
| Actual macOS bridge entry with metadata doubles | **102/102 PASS**, repeated under ASan/UBSan |
| Old-wrapper row-pitch reproducer | Expected failure observed; corrected wrapper passes |
| Identity fixtures / native source freeze | **10/10 PASS / 59 files PASS** |
| CPU bridge parity / formats / MFR | **PASS**, parity and concurrency max_err=0 |
| Real MSL compile/link and deliberate-error control | **PASS**, no shader execution implied |
| Downloaded source / logs / native artifact | **PASS**, exact hashes and source association |
| v0.9.9 AE / actual GPU render / runtime Build ID | **NOT RUN / NOT VERIFIED** |
| Full Cosmic equivalence / release | **Not approved** |

Source artifact **10998572690** contains 143 files; it matches the local tested
change and the two preserved upstream documentation blobs. Expected Git tree was
also verified before push. Source.tar.gz SHA-256:
`90291c40a81e48339317c25bfff177087e0d80dd55d250cbc7febaee1af23b71`.
Native log artifact **10999651464** was downloaded; all ten logs are present,
including old/new reproducer and entry sanitizer results. Its SHA-256 is
`6ad8144eb7030a5079f993a090a2a9788509bde677478fe99f6dbec396f20bf4`.
Rust/Cargo: 1.98.1; SDK: 15.5; Apple Metal: 32023.620. Unchanged MSL library:
70,206 bytes, SHA-256 `7625442fb9c92338f409140b3303d7860154f907b7666528198e14a3a869b27f`.
Machine-readable evidence: [METAL_099_VERIFICATION.json](evidence/METAL_099_VERIFICATION.json).

## Still open

Turbulence and Softness are not corrected. Existing 22 reference captures remain
available; no repeat diagnostic is requested. Depth/ROI rounding, Glow/Diffusion,
full presets/UI, numerical alpha/HDR and executed CPU/GPU parity, runtime identity,
clean install/restart/Undo/save/reopen/migration and real-host profiling remain open.
CPU bridge validation, device policy, aliased buffers and arbitrary invalid object
pointers are not covered by this Metal descriptor patch. No blanket GPU-safety
or performance improvement is claimed. Historical small CPU performance
regressions remain unresolved.

Previous v0.9.7 base-palette and v0.9.8 final-stage channel-weighted Grain fixes
are retained. The historical grain amplitude comparison is still limited to one
RGB8 fixture (3.079 local versus 3.072 Cosmic, with different random patterns).
The last actual user host tests exercised v0.9.6, not this candidate. No original
Cosmic binary/shaders/licensing code, user preferences/caches/security settings,
or main/Hot Loader branches were changed or published by this work.

Earlier reports are historical. The previous v0.9.8 checkpoint remains in Git
at `79e17721e612bd3c3d7aae457019256b4444c7c6`. This record supersedes only the
Metal-validation and build status for the exact candidate named above. Later
PR-merge or documentation-triggered artifacts must not silently substitute for it.

## v0.10.0 Turbulence + Softness native checkpoint — 2026-09-29

Native behavior commit: `16cfa398e7b0f1dd2701e3c8c40ba42e8bb42857`.
Governing rules re-read at blob `701a8c1ae3acb4dbfe1d7eda94acbf8095b88608`.
The fixed v0.10.0 patch passed its atomic apply/Level-1 workflow after the source-freeze manifest was regenerated from the exact post-patch inventory. The first attempt failed only at stale freeze hashes and is retained as historical FAIL evidence; it was not relabelled PASS.

Scope of the native commit: recovered 4-D Turbulence contract on CPU+Metal, corrected Size/Evolution/Amount mapping, Softness as a separate post-color smoothing stage, matching parameter hierarchy, dedicated Turbulence/Softness tests, regenerated Metal source, version 0.10.0, and updated frozen-source manifest. Retained base, Grain and Metal-buffer hardening remain in scope.

Local/reference evidence recorded by the commit reports RGB8 projected comparison against the retained real-AE Cosmic captures: Turbulence Size 3 MAE 0.0751 codes, Size 6 MAE 0.0739, Turbulence+Softness 40 MAE 0.7031. These are local CPU/reference projections, not a real AE/GPU PASS. Procedural CPU performance remains a recorded risk.

The native commit was created by the validation workflow bot, so GitHub marked its PR-triggered workflows `action_required` before any jobs existed. This documentation-only commit intentionally retriggers PR CI under the repository user without changing native source. The tested artifact identity must therefore use the exact CI head/build ID produced by the retriggered run; the native behavior delta remains exactly commit `16cfa398...`.

Release remains NOT APPROVED until Regression Level 2 real-AE checks pass on the exact built artifact.

## v0.10.0 CPU Turbulence performance checkpoint — 2026-10-01

This performance evidence was originally recorded while AE Development Rules v3.1.1 was current. On the same date the project migrated to **v4.0.0** commit `58d14aa12375757f4e52396d951f1557ae4f453c`, `AI_ENTRYPOINT.md` blob `29cb44eb7f2a4e0f7c120a97ae434dbb7e58174e`. Stellar remains an existing native effect in **Validation**; Cosmic now explicitly triggers the whole-product Reference Audit recorded in `REFERENCE_SPECIFICATION_COSMIC_V4.md` (PARTIAL).

The correct visual/math baseline remains v0.10.0 commit `16cfa398e7b0f1dd2701e3c8c40ba42e8bb42857`. Commit `9f3e73bc92534941db1106f521786d8c3c792347` optimizes only the CPU Turbulence path by precomputing regular-grid axis state. Metal is unchanged. A direct cached-renderer versus uncached `cosmic_fbm4` test was added. Strict core is **11/11 PASS**; ASan+UBSan and TSan are PASS.

Exact native-code Mac CI run `36911997718`: **8/8 jobs PASS**. Exact ARM64 artifact:
- Artifact ID: **11187002476**
- Artifact digest: `sha256:28ec6077bd5ed0019679d7a7d68e56083e2f4a0e4241020fbf60f2bafc8528b1`
- Plugin ZIP SHA-256: `4195641fe2b2a79e528cd1a80b3a10f33b278b3789fface7d71a80370a0e1c30`
- Build ID: `sg-0.10.0-9f3e73bc9253-clean-bc5efe6bf48a-aarch64-apple-darwin-36911997718.1`
- Source SHA-256: `bc5efe6bf48a18cd692ce5e5022fee84cf953ccc3821a35e18b1d3e33985cb35`
- Manifest release status: `NOT_APPROVED_AE_GATES_PENDING`

Performance instrumentation was added after the native change: `f488138` fixed the correct v0.10 baseline, `cc21f1e` added an absolute jitter floor, and `59448db` switched the gate to 11 paired baseline/current samples. These tooling commits do not change the native renderer.

Three completed paired measurements all show the 1280×720 procedural case faster than `16cfa398`:
- run `36912426549`, attempt 1: ratio **0.969**, delta **-3.311 ms**
- run `36912434745`, attempt 1: ratio **0.915**, delta **-14.318 ms**
- run `36912426549`, attempt 2: ratio **0.837**, delta **-19.610 ms**

Median raw paired ratio is **0.915**. Normalizing each procedural ratio by its unchanged 1280×720 base-path ratio gives approximately 0.919, 0.955 and 0.940; median approximately **0.940**. Because the hosted runner shows substantial timing variance, this is Level-1 synthetic CPU evidence only. It does **not** certify real After Effects or Metal performance.

Machine-readable record: [CPU_TURBULENCE_PERF_010_CI.json](evidence/CPU_TURBULENCE_PERF_010_CI.json).

Regression Level 2 is still **BLOCKED / NOT RUN** for this exact artifact: actual AE load and runtime identity, executed CPU/GPU numerical parity, 8/16/32 bpc and alpha/HDR/color-management paths, restart/Undo/Redo/save-reopen/migration, MFR/render queue/noninteractive paths, and real-host profiling. No merge, release or public delivery is approved by this checkpoint.

