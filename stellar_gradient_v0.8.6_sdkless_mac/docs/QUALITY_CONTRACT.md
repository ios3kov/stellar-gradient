# Quality contract

The audited CPU renderer in `src/cpu/ReferenceRenderer.cpp` running RGBA `float` is the visual reference.
Performance work must preserve the image; quality reduction is not an optimization target.

## Locked reference

`stellar_quality_reference` renders three deterministic Final-quality scenes (base, heavy, diffusion) and checks their F32 signatures. An intentional visual change must update these baselines only after image review.

## GPU acceptance gates

For Final quality, GPU readback against the CPU F32 reference must meet all of these on representative frames:

- finite output for every channel;
- max absolute error <= 1e-4 for base/palette/depth/grain;
- RMSE <= 2e-5 for base/palette/depth/grain;
- full-frame vs SmartFX-tile max absolute error <= 2.5e-3 for mip-filtered Glow/Diffusion edges;
- no structural difference caused by using FP16 intermediates in Final mode. Final therefore uses FP32 intermediates unless an FP16 path proves bit/epsilon-equivalent for the affected operation;
- Phase 0 and Phase 360 remain loop-equivalent;
- alpha remains premultiplied and Glow may expand alpha outside the input silhouette.

Preview may use lower precision only when the user explicitly selects Preview. Auto and Final are not allowed to silently trade quality for speed.
