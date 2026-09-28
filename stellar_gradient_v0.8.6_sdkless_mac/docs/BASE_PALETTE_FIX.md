# v0.9.7 — first native correction from isolated host evidence

2026-09-28. Development only; not a release or finished Cosmic parity.
Rules rechecked at FSTR-Line DEVELOPMENT_RULES.md blob
`a1760fde8763f789b50b91c20407938b4fcaea4a`.
Parent source tree: `6550d9959ed02762847350ab840a48885abaeec0`
(branch commit `61e82ab0f1281563e62891f97c32a8ba98674570`).
User still has the v0.9.6 test artifact from `deec788`; do not substitute a new
artifact for its historical evidence or ask to reinstall during this stage.

## Actual returned run

`Stellar-Isolate-1790622394083-275372080`, runner Isolate 1.0.
Archive SHA-256: `31dd998ea2f83854280a81d80217bfa115744c06a78ba4f60334a7486feed006`.
All 22 cases reported DONE/PASS, no captured host errors and cleanup PASS.
All 22 PNGs decode as RGB8 512x288, no alpha. AE 25.6x101, sRGB IEC61966-2.1,
linear blending false. Requested feature overrides read back successfully.
Loaded Build ID remains NOT_VERIFIED. CAPTURES_COMPLETE is not visual parity.
Full hashes and measurements: `diagnostics/evidence/isolate_1790622394083.json`.

The base case isolates the five-color gradient by setting Bulge, Turbulence,
Softness, Glow intensity, Grain and Diffusion blur to zero. Colors, Angle=90,
Cycles=1, Offset=0 and Phase=0 remain equal in both effects. No guess based on the
heavily textured combined default frame is needed anymore.

## Implemented, scoped mathematical correction

1. Base normalized phase was centered around zero, placing Color 1 halfway down
   the frame. Add the half-cycle origin outside the existing cycles multiplier.
2. Use pixel-origin positions, not the previous half-pixel offset: at the observed
   90-degree default, zero-offset unit-cycle coordinates are exactly y/height.
3. Interpolate linearly between the five cyclic working-space RGB colors instead
   of easing each segment with smoothstep. Color 5 still wraps to Color 1.

CPU and Metal use the same edits. Depth's independent center, the parameter ABI,
public IDs, defaults, UI, noise functions and blur passes are unchanged. No
proprietary executable or shader code was copied. CUDA is an unsupported old
prototype and is not part of this candidate; it is not claimed to match.

The generalized rotated/cycles/offset behavior is our explicit mathematical
contract; only the captured default-angle base is compared with actual Cosmic
in this stage. No inference of original behavior at untested settings is made.

## Checks and scope

Before changing production math, the new independent double-precision
`stellar_palette_geometry` test failed on the old library (54,750 mismatched
assertions). After the correction it passes. It checks unequal palette fractions,
wrap/negative phases, rotations, cycles/offsets, translated semantic bounds,
strided buffers, partial alpha and extended-range colors. Tolerance is 2e-5.
An optional `--write-base-f32 FILE` emits only a local 512x288 base CPU fixture.

Old CPU replay, projected through RGB8 quantization and sRGB output encoding,
matches the actually captured old Stellar base exactly on compared pixels.
Against Cosmic base, using the SAME projection and visible unoccluded pixels:

| Local replay vs captured Cosmic base | Before | After |
| --- | ---: | ---: |
| Mean absolute RGB output difference (0..255) | 58.283276 | 0.067527 |
| Maximum output-channel difference | 141 | 3 |
| RMS difference | 69.320155 | 0.305317 |

This is a measured single-fixture CPU correction, not zero-error equivalence,
not an AE execution of v0.9.7, not GPU readback and not color-management/HDR
certification. The red cross in Cosmic is kept intact; only a scalar-statistics
mask excludes it plus three pixels. No obscured pixels are reconstructed and
no cleaned/cropped reference image is generated or published.

The intentional image change also changes three composite F32 golden signatures.
Old values remain in Git history and the evidence record; new base/heavy/diffusion
outputs were inspected, retain finite HDR output and are now pinned with the
UNCHANGED tolerances. Updating the golden alone is not the correctness evidence;
the independent analytic test and actual reference comparison are separate.

Local strict core: 8/8 PASS. ASan+UBSan: 8/8 PASS. Native CPU bridge parity,
8/16/32 formats/HDR and concurrency checks PASS. Existing 10 identity tests and
static contracts PASS. New macOS Rust/package CI and Metal compilation must be
confirmed on the committed candidate; not assumed here. Actual GPU/AE v0.9.7:
NOT RUN. No performance improvement is claimed. Serial core benchmarks (3 runs,
with warm-up) are retained as sanity observations, not real AE profiling.

## Remaining confirmed mismatches — NOT FIXED by this stage

- Isolated Grain: mean absolute change from each renderer's own base is 3.072
  output codes for Cosmic versus 32.993 for installed Stellar.
- Turbulence: horizontal neighboring-channel variation is 0.229 vs 18.352
  (Size 3), and 0.115 vs 10.366 (Size 6). Spatial-scale/strength mismatch remains.
- Softness changes the original Depth-only output, while the two Stellar
  Depth fixtures are byte-identical. Its current use only as an FBM octave
  control cannot reproduce this observed behavior.
- Depth shape, Glow and Diffusion are not certified by fixing the base.
  Preset callbacks, group order, ROI rounding, runtime identity, full lifecycle,
  executed CPU/GPU parity and real-host profiling remain open.

Do not disguise those issues by zeroing defaults, arbitrarily dividing all noise
amounts, or claiming the complete effect is fixed from a base-only match.
No additional user run is needed to analyze the already returned feature data.

## Compatibility / rollback / reproduction

Version is 0.9.7 in Cargo and its root lock entry; PiPL/About derive that version.
An old project keeps its parameter IDs/values but WILL render a different base;
this is an intentional correction, not backward visual compatibility. Keep the
previous test artifact for controlled rollback. No installation is performed here.

From the committed clean source: run CMake/CTest as in Mac CI; run
`stellar_palette_geometry --write-base-f32 new.f32`. Compile the same test against
the parent library to produce old.f32. With the original private archive available:

```
python diagnostics/analyze_isolation.py ORIGINAL.zip --old-base old.f32 --new-base new.f32 --out measurements.json
```

The analyzer needs NumPy/Pillow, does not extract arbitrary archive paths, modifies
no inputs, and writes only its explicit output JSON. Linux little-endian float32
replay is the measured environment. The shader's tracked embedded header is
regenerated with `tools/embed_metal.py`; the source freeze is updated for the
intentional change, not relaxed. Evidence stays separate from the native payload.

Research: Adobe's documented pixel-origin sampling convention and floating-point
color suite support the independent coordinate/precision tests; neither document
specifies Cosmic's algorithm. Algorithmic conclusions above come from the actual
supplied frames, not the SDK guide.
https://ae-plugins.docsforadobe.dev/effect-details/graphics-utility-suites/
https://ae-plugins.docsforadobe.dev/effect-details/parameters-floating-point-values/
