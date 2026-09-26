# Clean-room decomposition notes

## Inputs inspected

- `Cosmic.aex` Windows x64 binary supplied by the user.
- `Cosmic v1.0` one-page installation/getting-started PDF supplied by the user.
- Public aescripts Cosmic product page.

## Binary facts

- PE32+ x86-64 DLL / `.aex`.
- SHA-256: `b1b55fc6f0795dad45dfd1e79bfd180eb6d881519aade4250bd6416d737a3ea8`.
- Exported Adobe entry points include `EffectMain` and `PluginDataEntryFunction2`.
- The Windows binary contains an NVIDIA fatbin section and 21 distinct CUDA kernel names.
- Licensing exports/strings are from the aescripts licensing framework. They are explicitly out of scope for this clean-room implementation.

## Observed controls

### Palette / gradient
- Palette
- five editable colors
- Angle
- Cycles
- Offset
- Phase
- Saturation
- Brightness

Observed palette names in the binary:
- Retro Pop
- Sage
- Blush
- Deep Space
- Electric
- Ultraviolet
- Lagoon
- Sunset
- Pride Rainbow
- Candy

### Depth
- Contrast
- Bulge
- Rounding

### Turbulence
- Amount
- Size X
- Size Y
- Evolution
- Softness

### Glow
- Radius
- Falloff
- Threshold
- Intensity
- Soft Clip

### Grain
- amount (group master)
- Size
- Color
- Animate

### Optical diffusion
- Blur
- Center
- Focus
- Feather
- Invert

## GPU pipeline inferred from kernel names

Distinct kernel names found in the supplied Windows binary:

1. `CosmicShapeKernel`
2. `CosmicRowBoundsKernel`
3. `CosmicBoundsReduceKernel`
4. `CosmicDepthKernel`
5. `CosmicBoxBlurHKernel`
6. `CosmicBoxBlurVKernel`
7. `CosmicColorizeKernel`
8. `CosmicMaskKernel`
9. `CosmicGlowSourceKernel`
10. `CosmicDownsampleKernel`
11. `CosmicRowMaxKernel`
12. `CosmicMaxReduceKernel`
13. `CosmicNormalizeKernel`
14. `CosmicUpsampleAccumulateKernel`
15. `CosmicGlowCombineKernel`
16. `CosmicCoverageKernel`
17. `CosmicDiffRadiusKernel`
18. `CosmicSatRowKernel`
19. `CosmicSatColKernel`
20. `CosmicVarBlurKernel`
21. `CosmicCompositeKernel`

A plausible render graph is therefore:

`alpha -> shape -> bounds reduction -> depth field -> blur -> colorize -> alpha mask -> glow source -> downsample pyramid -> max/normalize -> upsample accumulate -> glow combine -> diffusion radius -> SAT horizontal/vertical -> variable blur -> composite`

This is an inference from observable symbols, not copied implementation logic.

## Clean-room redesign

The implementation intentionally uses a different render graph:

- Stable SmartFX layer-space coordinates so tiled renders do not restart procedural patterns.
- Fused Depth + Turbulence + palette + mask + Grain base pass.
- Glow threshold pass followed by only the mip levels that can actually be sampled.
- Mip-LOD Optical Diffusion instead of the observed SAT horizontal/vertical variable-blur chain.
- Aligned internal work regions provide blur neighborhoods while the actual SmartFX output remains clipped to the host request.
- Glow + Diffusion reuse the composed base texture instead of allocating a third full mip chain.
- Preview/Auto Metal intermediates use RGBA16F; Final uses RGBA32F.
- Disabled modules are skipped completely.
- CPU and Metal share the same public parameter model; CPU is a real fallback, not a passthrough.

Future profiling may justify persistent per-device texture pools, but v0.3 deliberately keeps transient texture ownership simple until the first real AE/Metal host measurements are available.
