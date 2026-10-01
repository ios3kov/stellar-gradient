# Cosmic Reference Specification — AE Development Rules v4

Date: 2026-10-01  
Project branch: `fix/stellar-release-gate`  
Reference Audit result: **PARTIAL**  
Current delivery purpose: **Validation**, not release.

This document migrates the existing Stellar Gradient evidence to the mandatory
Reference Audit contract introduced by AE Development Rules v4.0.0. It does not
retroactively upgrade old tests, invent missing evidence, or authorize new
invasive analysis.

## 1. Reference identity

| Field | Value |
| --- | --- |
| Product / reference | Loophouse **Cosmic** |
| Runtime registration observed | **Cosmic 1.0x1** |
| Supplied static artifact | `Cosmic.aex` |
| Static artifact format | PE32+ DLL, Windows x86-64 |
| Static artifact SHA-256 | `b1b55fc6f0795dad45dfd1e79bfd180eb6d881519aade4250bd6416d737a3ea8` |
| Static artifact size | 940032 bytes |
| Runtime host used for retained captures | After Effects **25.6x101** |
| Runtime OS reported | Macintosh OS **26.6.2/64** |
| Runtime project color setup | sRGB IEC61966-2.1, linear blending false |
| Reference received | 2026-09-28 |
| Main identity evidence | `diagnostics/reference_receipt.json`, `diagnostics/HOST_RUN_1790619232985.md` |
| Analysis boundary | Licensing/activation bypass and proprietary code/assets redistribution are out of scope. Existing static findings are retained as historical project evidence. Any **new** decompilation/disassembly beyond the already documented evidence is BLOCKED unless the required authorization is explicitly documented. |

**Reference Claim Status**

- Windows file identity/hash/architecture: **PROVEN**.
- Registration of Loophouse Cosmic 1.0x1 in the retained macOS AE session: **OBSERVED**.
- Exact package hash/architecture of the installed macOS Cosmic build: **UNKNOWN**.

## 2. Reference scope

Reference mode: **whole-product behavior/render reference** for the Cosmic effect.

Target scope:

- user-visible effect hierarchy and controls;
- defaults, ranges and units;
- built-in palettes/preset callback behavior;
- Depth, Turbulence, Softness, Glow, Grain and Optical Diffusion behavior;
- state/lifecycle behavior relevant to AE projects;
- render/output behavior, alpha/color/bit depth, CPU/GPU/MFR;
- real-host performance envelope.

Explicit non-scope:

- Cosmic licensing/activation implementation;
- redistribution of Cosmic executable/shader/assets;
- copying proprietary internal architecture;
- exact reference random seed/pattern for Grain;
- Stellar-only `Render Engine` and `Quality` controls.

Stellar is an independent implementation. Internal architecture may differ as long
as the agreed user-visible contract is met.

## 3. Evidence sources

| ID | Type | Source | What it supports | Limitation |
| --- | --- | --- | --- | --- |
| REF-E001 | static identity | `diagnostics/reference_receipt.json` | file hash, format, platform | Windows artifact only |
| REF-E002 | static inspection | `docs/DECOMPOSITION.md`, `docs/COSMIC_BINARY_AUDIT.txt` | control inventory, exports, observable CUDA kernel names | does not prove macOS internals |
| REF-E003 | runtime | `diagnostics/HOST_RUN_1790619232985.md` | Cosmic 1.0x1 registration, AE/OS environment | smoke output was mainly Stellar |
| REF-E004 | paired runtime | `diagnostics/COMPARE_1790620677995.md` + JSON | 35 common defaults, paired default renders | RGB8 only; watermark/overlay present |
| REF-E005 | paired runtime | `diagnostics/evidence/isolate_1790622394083.json` | 22 feature-isolation captures | RGB8 only; historical Stellar 0.9.6 |
| REF-E006 | measured replay | `docs/BASE_PALETTE_FIX.md` | base gradient geometry correction | local CPU projection, not exact-candidate AE |
| REF-E007 | static + measured replay | `docs/GRAIN_CORRECTION.md` | Grain envelope/stage evidence | reference random pattern remains different |
| REF-E008 | static + measured replay | `docs/TURBULENCE_SOFTNESS_010.md` | recovered 4-D Turbulence and Softness evidence | exact candidate real AE/GPU not yet run |
| REF-E009 | historical static recovery | `docs/STATUS.md` | UI defaults/ranges, palette callback, Depth/Rounding recovery | claims are scoped to inspected Windows artifact |
| REF-E010 | current candidate CI | PR #1 + Mac CI run 36911997718 | current Stellar build/identity/code-side regression | not reference runtime evidence |

## 4. UI / control inventory

The current target UI contract is based on static reference evidence and the
retained runtime parameter snapshots.

### Palette — expanded by default

| Control | Type | Reference default / range | Claim |
| --- | --- | --- | --- |
| Presets | popup | Cold, Retro Pop, Sage, Blush, **Deep Space**, Electric, Ultraviolet, Lagoon, Sunset, Pride Rainbow, Candy, separator, Custom; Deep Space default | **PROVEN** static |
| Color 1..5 | color | Deep Space default: [40,13,140], [55,17,191], [24,14,89], [7,12,38], [242,75,75] | **PROVEN** static |
| Angle | angle | 90° | **PROVEN** static |
| Cycles | slider | valid 0.1..20, slider 0.1..5, default 1 | **PROVEN** static |
| Offset | percent | -100..100%, default 0% | **PROVEN** static |
| Phase | angle | 0° | **PROVEN** static |
| Saturation | percent | 0..200%, default 100% | **PROVEN** static |
| Brightness | percent | valid 0..400%, slider 0..200%, default 100% | **PROVEN** static |

### Depth — collapsed by default

| Control | Reference default / range | Claim |
| --- | --- | --- |
| Angle | 0° | **PROVEN** static |
| Contrast | valid 0..400%, slider 0..200%, default 100% | **PROVEN** static |
| Bulge | valid -200..200%, slider -100..100%, default 60% | **PROVEN** static |
| Rounding | 0..100%, default 100% | **PROVEN** static |
| Turbulence / Amount | valid 0..500, slider 0..200, default 40 | **PROVEN** static |
| Turbulence / Size X,Y | valid 0.1..50, slider 0.1..10, default 3 | **PROVEN** static |
| Turbulence / Evolution | angle, default 0° | **PROVEN** static |
| Softness | valid 0..1000, slider 0..200, default 40 | **PROVEN** static |

Observed hierarchy from the reference parameter sequence places **Turbulence
inside Depth**, closes Turbulence, exposes **Softness**, then closes Depth.

### Look — collapsed by default

| Group / control | Reference default / range | Claim |
| --- | --- | --- |
| Glow / Radius | valid 0..2000, slider 0..600, default 194 | **PROVEN** static |
| Glow / Falloff | 0..100%, default 50% | **PROVEN** static |
| Glow / Threshold | 0..100%, default 0% | **PROVEN** static |
| Glow / Intensity | valid 0..400%, slider 0..200%, default 160% | **PROVEN** static |
| Glow / Soft Clip | 0..100%, default 0% | **PROVEN** static |
| Grain / Amount | 0..200%, default 20% | **PROVEN** static |
| Grain / Size | valid 0.3..5, slider 0.3..3, default 1 | **PROVEN** static |
| Grain / Color | 0..100%, default 100% | **PROVEN** static |
| Grain / Animate | checkbox, default On | **PROVEN** static |
| Optical Diffusion / Blur | valid 0..2000, slider 0..500, default 15 | **PROVEN** static |
| Optical Diffusion / Center | point, default 50/50 | **PROVEN** static |
| Optical Diffusion / Focus | valid 0..4000, slider 0..1000, default 50 | **PROVEN** static |
| Optical Diffusion / Feather | valid 0..4000, slider 0..1000, default 450 | **PROVEN** static |
| Optical Diffusion / Invert | checkbox, default Off | **PROVEN** static |

The retained paired runtime comparison independently observed **35 common editable
default values matching** between the then-installed Stellar and Cosmic. That is
**OBSERVED** host evidence, not proof of every static range or callback.

## 5. Preset contract

- Built-in menu inventory above: **PROVEN** static.
- Exact five color values for all 11 built-ins are retained in the project host
  table and static verifier: **PROVEN** for the inspected Windows artifact.
- Selecting a built-in Cosmic palette changes **Color 1..5 only**; non-color
  controls remain unchanged: **PROVEN** from the inspected `USER_CHANGED_PARAM`
  path and re-verification recorded in `docs/STATUS.md`.
- Manual Color 1..5 edits switch the menu to Custom: **PROVEN** static.
- Runtime save/load portability of custom presets across versions: **UNKNOWN**.

## 6. Functional / render claim ledger

| Claim | Status | Evidence / boundary |
| --- | --- | --- |
| Base five-knot palette is cyclic and the retained default fixture uses pixel-origin geometry matching the corrected Stellar base | **OBSERVED** | REF-E004/006; single primary host fixture plus local replay |
| Windows Cosmic contains a directional Depth CUDA path and separate box-blur Rounding path | **PROVEN** | static PTX/path evidence recorded in `docs/STATUS.md` |
| The installed macOS reference necessarily uses the same internal CUDA-derived implementation | **UNKNOWN** | no macOS reference binary/static identity |
| Windows Cosmic Turbulence exposes deterministic 4-D improved-Perlin behavior with two 3-octave displacement fields | **PROVEN** for inspected Windows CUDA path | REF-E008 |
| macOS reference output at Size 3/6 follows the same external behavior closely enough to constrain Stellar | **OBSERVED** | REF-E005/008 |
| Softness is a separate user-visible smoothing stage and changes output even with isolated Depth | **OBSERVED** | REF-E005 |
| Exact host-side Softness dispatch formula | **INFERRED** | output comparison + observable blur pipeline; not directly proven |
| Grain uses a channel-dependent final composite envelope | **PROVEN** for inspected Windows path | REF-E007 |
| Exact Cosmic Grain PRNG/seed/pattern | **UNKNOWN** | intentionally not reproduced |
| Glow and Optical Diffusion complete visual parity | **UNKNOWN / not claimed** | historical isolation shows remaining differences |
| Exact current Stellar v0.10 behavior inside real AE/Metal | **UNKNOWN until Level-2 validation** | current artifact has only code-side CI |

## 7. State / persistence and animation

Current reference coverage is insufficient for a whole-product parity claim.

| Scenario | Coverage / claim |
| --- | --- |
| Initial defaults | **OBSERVED / PROVEN** as scoped above |
| Reset | **PARTIAL** |
| Duplicate / copy-paste | **UNKNOWN** |
| Undo / redo | **UNKNOWN** |
| Project save / reopen | **UNKNOWN** |
| AE restart | **UNKNOWN** |
| Remove / re-add | **UNKNOWN** |
| Version migration | **UNKNOWN** |
| Keyframes / interpolation | **UNKNOWN** |
| Expressions | **UNKNOWN** |
| Grain temporal repeatability | **PARTIAL**; reference exact seed mapping unknown |

These items are part of the pending real-host Validation gate; they are not
silently treated as PASS.

## 8. Alpha / color / bit depth

Retained paired Cosmic captures are RGB8 PNG and contain no alpha channel.
Therefore:

- Cosmic 8-bpc RGB appearance for the retained fixtures: **OBSERVED**.
- Cosmic alpha/premultiplication behavior: **UNKNOWN**.
- Cosmic 16/32-bpc, HDR/negative/extended-range behavior: **UNKNOWN**.
- Current Stellar host/bridge contracts cover these code paths, but that is not
  reference parity evidence.

## 9. Performance baseline

No valid real-AE paired **Cosmic vs exact current Stellar v0.10** performance
baseline exists yet.

Current Stellar CPU optimization has Level-1 synthetic evidence only. It must not
be presented as Cosmic performance superiority.

Reference performance coverage: **BLOCKED pending controlled real-host measurement**.

## 10. Packaging / integration

- Windows reference artifact identity/package: **PROVEN** for the supplied file.
- macOS Cosmic registration in AE 25.6: **OBSERVED**.
- exact installed macOS package hash/signing/dependencies: **UNKNOWN**.
- licensing/activation internals: **N/A / out of scope**.
- no bypass, modification or redistribution of the reference is allowed by this
  project contract.

## 11. Reference Coverage Map

| Area | Coverage | Key claim status | Gap / impact |
| --- | --- | --- | --- |
| Identity / package | **PARTIAL** | PROVEN Windows / OBSERVED Mac registration | macOS package identity unknown |
| UI / interaction | **COMPLETE for material effect controls** | PROVEN static + OBSERVED defaults | keyboard/tooltips/minor AE UI behavior not separately audited |
| Parameters / functionality | **PARTIAL** | mixed PROVEN/OBSERVED | Glow/Diffusion and several interactions not certified |
| Presets | **PARTIAL** | PROVEN static | runtime save/load/version portability unknown |
| State / persistence | **BLOCKED** | UNKNOWN | requires real AE lifecycle matrix |
| Animation / keyframes / expressions | **BLOCKED** | UNKNOWN | requires real AE |
| Render / output | **PARTIAL** | OBSERVED | exact current v0.10 host run pending |
| Alpha / color / bit depth | **BLOCKED** | UNKNOWN beyond RGB8 fixture | 16/32/HDR/alpha pending |
| Edge cases / errors | **PARTIAL** | OBSERVED for harness failures | full reference edge matrix absent |
| Performance | **BLOCKED** | UNKNOWN for exact paired host run | real AE profiling pending |
| Compatibility / host behavior | **PARTIAL** | OBSERVED AE 25.6 macOS | broader matrix not claimed |
| Packaging / integration | **PARTIAL** | PROVEN Windows / OBSERVED Mac registration | installed Mac package details unknown |
| Internal implementation claims | **PARTIAL** | PROVEN only where scoped to inspected Windows binary | macOS internal equivalence must remain UNKNOWN |

## 12. Target contract for Stellar Gradient

| Target | Desired behavior | Exact parity? | Allowed difference |
| --- | --- | --- | --- |
| UI hierarchy/defaults/ranges | reproduce documented Cosmic effect controls | **yes** for common controls | Stellar Engine/Quality additions |
| Built-in palettes | same names/order/colors and callback semantics | **yes** | none for common palette behavior |
| Base gradient | reproduce reference external result | **yes**, host tolerance must be fixed before final PASS | independent implementation |
| Depth / Rounding | reproduce external behavior | **yes** | internal algorithm/backend may differ |
| Turbulence / Softness | reproduce external behavior | **yes** | independent implementation |
| Grain | reproduce amount/color envelope and stage semantics | **no pixel-random parity** | Stellar PRNG/seed/pattern may differ |
| Glow / Diffusion | reproduce user-visible behavior | **target yes, current status open** | internal render graph may differ |
| State/lifecycle | behave safely and predictably in AE | **yes** for agreed scenarios | internal serialization details may differ |
| Alpha/color/bit depth | equivalent visual/numeric semantics | **yes** within fixed tolerances | backend implementation may differ |
| Performance | measure honestly against reference | no unmeasured superiority claim | optimization strategy may differ |
| Licensing/activation | not copied | **N/A** | wholly out of scope |

## 13. Parity acceptance tests

| ID | Scenario | Comparison | Gate |
| --- | --- | --- | --- |
| PAR-001 | common control tree/defaults/ranges | static contract + real AE snapshot | Validation |
| PAR-002 | 11 built-in palettes + Custom behavior | values and changed/unchanged parameters | Validation |
| PAR-003 | base / Depth / Turbulence Size 3 & 6 / Softness | paired same-fixture captures; exact-candidate tolerance fixed before PASS | Validation |
| PAR-004 | Grain Amount 20 / Size 1 / Color 100 | envelope/statistics; random pixel identity explicitly not required | Validation |
| PAR-005 | Glow and Optical Diffusion isolation | paired same-fixture captures | Validation |
| PAR-006 | 8/16/32 bpc + alpha/HDR/color-management | paired numeric/visual comparison | Validation |
| PAR-007 | restart, Undo/Redo, save/reopen, duplicate/copy | state equivalence and no corruption | Validation |
| PAR-008 | CPU/GPU, MFR, preview/render queue/noninteractive | output parity and stability | Validation |
| PAR-009 | representative real-host performance | repeated paired timings, same environment/cache policy | Validation |
| PAR-010 | final exact artifact identity | loaded Stellar Build ID must match candidate | Validation |

No historical v0.9.6 capture is allowed to certify the current v0.10 artifact.

## 14. Known gaps

1. Exact macOS Cosmic package identity is unavailable.
2. Current v0.10 exact artifact has not yet been executed in real AE.
3. State, animation, alpha/HDR and real-host performance coverage are incomplete.
4. Glow and Optical Diffusion parity remain open.
5. Exact Grain random pattern is intentionally outside the target.
6. Future invasive reference analysis is not authorized by this migration record;
   use existing evidence or obtain/document the required authorization first.

## 15. Exit / handoff

Reference Audit status: **PARTIAL**.

It is sufficient to continue the already-planned **Validation evidence collection**
because the remaining gaps are explicitly the subjects of that validation. It is
**not sufficient for a whole-product parity or Release claim**.

Next step: run the Level-2 real-AE validation matrix on the exact identified
Stellar v0.10 artifact, then update this Coverage Map from actual evidence.
