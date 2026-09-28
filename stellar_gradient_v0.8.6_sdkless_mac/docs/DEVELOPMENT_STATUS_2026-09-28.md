# Stellar Gradient — verified checkpoint, 2026-09-28

**Not a release.** Work branch: `fix/stellar-release-gate`, draft PR #1. Main and
Hot Loader branches remain unchanged. The user still has the v0.9.6 test build;
no v0.9.8 installation or actual AE/GPU execution has occurred in this stage.
This is a post-build documentation record, not a new native candidate.

## Last code-side validated candidate

- Version: **0.9.8**.
- Commit: `d7e035ddf8751deb1c43c54d1cff050036890d64`.
- Clean source tree: `275f541753a4c403beb2a50d73539a4f51be6ecf`.
- Build ID: `sg-0.9.8-d7e035ddf875-clean-265b2ab6504b-aarch64-apple-darwin-36483266159.1`.
- Push CI: https://github.com/ios3kov/stellar-gradient/actions/runs/36483266159 .
- Internal ARM64 artifact: **10997866159**.
- Artifact ZIP SHA-256: `73046dc7f015901308a3892b0685f35c3004ac89e92f7d783a945a6f2f0bdb56`.
- Plugin ZIP SHA-256: `0fb4285b7fe8fe0824f12dfa3b2d62d9be584aa5c019eb0c145ba36d156e5aec`.

The downloaded artifact's six payload files, embedded version/commit/Build ID,
ARM64 Mach-O header, and all 27 compiled-source hashes were independently checked.
The source artifact 10998160700 reproduces all 138 files of the tested tree;
its source.tar.gz SHA-256 is
`da6c252a36ddd74c9807a5bd6eea92899f48728556299c1c9c5ae924aad37a7a`.
No downloaded binary was edited or re-signed. Ad-hoc signature validation is not
notarization or proof that the user's AE loaded this binary.

## Work completed in this continuation

The previous local v0.9.8 grain patch is now published in the work branch
(code restore commit `0ee59603e3f241885d54f8ca749f865effaa3c50`). All source bytes
were verified against the original saved patch; nothing was refitted or discarded.
The correction keeps v0.9.7's base palette and applies channel-weighted Grain once,
after Glow/Diffusion. Random field, defaults, parameter IDs and ABI are unchanged.
Temporary transfer files and workflow have been removed from the current tree.

CI now compiles the actual Metal shading language source, not only the ObjC++
wrapper containing its string. The required negative shader control also runs.
An additional evidence defect was found during validation: upload-artifact ignored
logs under `.ci-native`. The exact log glob now explicitly includes hidden files
and missing logs fail the upload. The final artifact 10997326583 was downloaded:
all six logs are present, including compilation and deliberate-error diagnostics.
No broad hidden-directory upload or user-machine security change is involved.

## Observed checks

| Check | Status and scope |
| --- | --- |
| CI source/static/strict/ASan+UBSan/TSan/native/Rust/package jobs | **PASS, 8/8** at the exact candidate above |
| Core suite | **PASS, 9/9** in each CI core mode; local strict rerun also 9/9 |
| Build identity fixtures | **PASS, 10/10**; no real AE session implied |
| Native CPU bridge parity and concurrency | **PASS**, max_err=0; no GPU execution implied |
| Actual MSL compile + library link | **PASS**, fast math disabled; Apple Metal 32023.620, SDK 15.5 |
| Deliberately invalid shader control | **PASS**, compiler rejects expected #error |
| Source/header/grain placement and freeze | **PASS**, 56 frozen native-project inputs |
| Downloaded source, log and binary artifact integrity | **PASS**, checked against exact commit and manifests |
| New v0.9.8 AE session / real GPU dispatch | **NOT RUN** |
| Full Cosmic visual equivalence / release | **Not approved** |

The Metal IR library is 70,206 bytes; SHA-256
`7625442fb9c92338f409140b3303d7860154f907b7666528198e14a3a869b27f`.
The compiled source SHA-256 is
`828e90210a1c1693987fa74a6cd59973df802a77b7d7baf2e5cf4f32896b9680`.
Native logs artifact SHA-256:
`a6b5826f08034ec2017f3fbec26b95ea721a9ad6e0b6f0af5294462968dba484`.
Rust and Cargo were 1.98.1. These recorded toolchains do not imply hardware-wide
reproducibility or compatibility with all older macOS/Metal versions.

## Remaining work, not concealed by green CI

1. Establish the reference's host-to-noise coordinate mapping, then correct and
   independently test Turbulence and Softness. No guessed scale factor or new
   blur has been added. The existing captured 22-case isolation data is retained.
2. Resolve Depth/ROI rounding, Glow/Diffusion and complete presets/UI differences.
3. Harden bridge input validation. Static review found that the Metal row-byte
   expression `rowbytes % sizeof(float) * 4` tests divisibility by four, not by a
   complete 16-byte pixel. This requires a reproducer and correction before the
   next native handoff; no real-host failure is asserted from this observation.
4. Verify the exact new loaded Build ID, real CPU/GPU and alpha/HDR output,
   lifecycle/restart/Undo/save/reopen/migration, then real-host performance.

Historical grain measurements remain limited to one static RGB8 fixture: mean
absolute Grain change 3.079 for local v0.9.8 versus 3.072 for Cosmic; their random
patterns differ. Historical short CPU full-render timings regressed in some cases;
this continuation does not fix or reclassify that cost. Old v0.9.6 AE evidence is
not reused as v0.9.8 runtime proof. No user rerun or reinstall is requested here.

## Evidence precedence

This current post-build record supersedes the old publication/Mac-NOT-RUN lines
in the earlier source-stage `GRAIN_CORRECTION.md` and the pre-build
`CHECKPOINT_098_MAC.md` only for the candidate explicitly identified above.
Earlier reports remain historical. Later documentation-only commits do not
relabel this binary, its Build ID, or any PR-merge artifact.
