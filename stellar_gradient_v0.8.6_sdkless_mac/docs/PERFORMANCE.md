# Performance

## Current design

- Base-only: direct GPU output, one compute pass.
- Base + Glow seed: fused full-work-region pass.
- Glow/Diffusion: minimal required mip chain; disabled modules do no work.
- Frame constants (direction, inverse scales, LODs, centers, thresholds) are computed once on CPU, not per pixel.
- Depth `pow()` is skipped completely when Bulge is zero.
- SmartFX output stays at the requested tile; only the required aligned halo is rendered internally.
- Metal pipelines compile once per GPU device setup.
- Per-frame parameter `MTLBuffer` allocation was removed (`setBytes` for the small constant block).
- Preview may use RGBA16F intermediates; Auto and Final use RGBA32F.
- CPU uses a persistent worker pool and thread-local reusable mip storage.

## CPU reference/regression numbers

Linux audit container only; these are not Mac targets. Current 1280x720 values vary with shared-host scheduling: roughly base 9-16 ms, procedural 42-60 ms, heavy/full 160-190 ms. The real production target is Metal inside After Effects.

## Mac target matrix

See `BENCHMARK_COSMIC.md`.
