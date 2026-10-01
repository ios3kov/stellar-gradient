# v0.9.8 publication and actual Metal-source compilation

2026-09-28. Governing rules: FSTR-Line DEVELOPMENT_RULES.md, blob
`a1760fde8763f789b50b91c20407938b4fcaea4a`. Work branch remains
`fix/stellar-release-gate`; main and Hot Loader branches are not changed.

## Baseline and accepted scope

The previously supplied v0.9.8 patch was NOT in GitHub. This stage restores it
without losing or refitting its grain correction, then validates the Mac build.
It does not change Turbulence/Softness or replace the user's installed v0.9.6.
The reference captures already returned are sufficient for further investigation;
no repeated user diagnostic is required in this stage.

Acceptance fixed before CI changes:

- Restore the previously tested source bytes, not a handwritten reconstruction.
- Repeat strict core, grain pipeline, source freeze and identity tests.
- Compile the actual Metal source embedded by the runtime, using the Apple Metal
  compiler with fast math disabled, and produce a nonempty Metal IR library.
- Deliberately invalid shader input must fail with its expected diagnostic.
- Existing Mac/CPU/sanitizer/Rust checks must still pass before packaging.
- Verify the new artifact's own source/commit/Build ID/hashes after signing.
- Keep actual GPU/AE execution NOT RUN, rather than treating compilation as a render.

## Source restoration evidence

Original patch SHA-256:
`d9ff7a1f7625fce56ef9fbf5656ce948ec7b5c2380389b8232f3cfb941699065`.
Imported upstream tree: `26b9edd668e644edf3b6c819e69094b7a5856f4e`.
The restored local full tree is byte-identical to the earlier v0.9.8 checkpoint:
`7e11da5df9bd3237a2c2d9303157ae0e81cc2c96`.

Remote code commit `0ee59603e3f241885d54f8ca749f865effaa3c50` restores the
reviewed source; the temporary transfer job 36482516892 succeeded. It verified
exact README, diagnostics and native-project Git object hashes, regenerated the
Metal header, and reran strict core 9/9, ten identity tests and static contracts.
The transport patch excluded workflow edits; those are applied normally by the
connector. All temporary transport chunks/workflow are removed in this checkpoint.
No original binary, original shader, reference image or license code is published.

`GRAIN_CORRECTION.md` and `evidence/GRAIN_098_LOCAL.json` preserve the earlier
local-stage evidence, including its then-read-only GitHub limitation. That old
publication limitation is superseded by this record, not retroactively erased.
The earlier 3.079 versus 3.072 grain-amplitude result remains a single captured
RGB8 case, with different random fields; it is NOT exact reference parity.
Previous short CPU timings showed some regressions; no performance fix is claimed.

## Why a new compiler gate is needed

Previously Native bridge + Metal compiled MetalBridge.mm, which passes a shader
string to newLibraryWithSource at runtime. Compiling that Objective-C++ file does
not compile the Metal language inside the string. Static header/placement checks
also do not detect shader compiler errors.

The new native-mac step runs .github/scripts/compile-metal.sh on the exact tracked
.metal source. The existing header verifier establishes that its bytes match the
embedded runtime source. Apple tools compile to AIR and link an MTLB library;
the script checks the library header, records SDK/compiler/hash information and
runs a required negative #error control in a scratch directory. Failures block
packaging through the existing native-mac dependency. Compilation logs are kept
as artifacts even on failure. No new metallib is substituted into the plugin;
runtime loading, render math, ABI, controls and dependency versions are unchanged.

The test uses the SDK's default language version, as the runtime currently does.
It is not proof for every older SDK/OS, a real GPU dispatch, CPU/GPU numerical
parity, runtime pipeline creation, signing-policy approval or an AE session.

## Current validation status at implementation

Local: restored strict core 9/9 PASS; YAML dependency/trigger and shell syntax
checks PASS. The new actual Metal-source check and Mac artifact gates are pending
execution. Exact observed CI/artifact results are recorded in PR #1 after the run;
they must not be inferred from this pre-build document.

## Turbulence/Softness investigation boundary

The current Stellar implementation feeds two-dimensional value noise directly
into the palette coordinate, with pixel Size scaling and Softness controlling
an octave count. Private read-only reference inspection instead shows separate
spatial displacement and looping evolution coordinates; mapping the host's UI
values to those shader arguments is not yet fully established. This is an
investigation result, not a tested replacement algorithm. The observed Softness
response in the Depth-only reference remains unexplained. No fitted scale factor,
noise replacement or arbitrary blur is introduced just to resemble one frame.

## Primary research

Apple Metal library compilation (AIR is GPU-independent; execution remains separate):
https://developer.apple.com/documentation/metal/building-a-shader-library-by-precompiling-source-files
https://developer.apple.com/documentation/metal/metal-libraries
Runtime compile options and default language version:
https://developer.apple.com/documentation/metal/mtlcompileoptions
https://developer.apple.com/documentation/metal/mtlcompileoptions/languageversion
