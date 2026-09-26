# Performance-only audit (v0.3 -> v0.4)

Scope: preserve the locked CPU F32 image contract while removing avoidable work.

## Measured CPU baseline in the audit container

Release build, synthetic benchmark (`tests/core_bench.cpp`), two runs:

| 1280x720 case | run 1 | run 2 |
|---|---:|---:|
| base | 21.4 ms | 20.2 ms |
| turbulence + grain | 56.4 ms | 65.8 ms |
| full (turbulence + grain + Glow + Diffusion) | 188.5 ms | 182.9 ms |

These numbers are not Mac/AE targets; they are a repeatable code-regression baseline for this environment.

## GPU hot spots found

1. `MTLBuffer` for parameters is allocated every frame.
2. `base` and `glow` textures are allocated every frame instead of being reused per GPU device/size/format.
3. Base shading and Glow thresholding are separate full-resolution passes. They can be fused without changing math.
4. `cos(angle)`, `sin(angle)` are evaluated in every pixel in Metal although angle is frame-constant.
5. `log2(glow_radius)` and `log2(diffusion_blur)` are evaluated in output pixels although the LODs are frame-constant.
6. Depth `pow()` runs even when Bulge is zero; in that state depth cannot affect the image and must be skipped.
7. Several constant normalization values (bounds center/reciprocal width/height, glow spread) can be packed once on CPU.
8. Runtime Metal source compilation is correctly device-setup-only, not per frame. Pipelines are already cached for device lifetime.
9. One command buffer is already used for the complete render, with no explicit CPU wait; keep this property.
10. Built-in mip generation is fast but its cross-family numerical behavior must be validated against the quality contract before Final mode relies on it.

## CPU hot spots found

1. Per-frame full-resolution Glow source allocation.
2. Per-frame allocations for every mip level.
3. `parallel_rows()` creates/join OS threads on every substantial pass; Glow + Diffusion therefore repeatedly pay thread startup costs.
4. Base shading evaluates depth `pow()` when Bulge is zero.
5. The mip builder always owns independent vectors; a reusable scratch/workspace will reduce allocator pressure.
6. Procedural turbulence is compute-heavy by design; optimize arithmetic/SIMD after memory/thread overhead is removed.

## Priorities

P0 — mathematically equivalent:
- precompute frame constants;
- skip dead features exactly;
- fuse base + Glow seed;
- persistent GPU param buffer;
- reusable GPU texture pool;
- CPU render workspace;
- persistent CPU worker pool or a host-aware low-overhead parallel executor.

P1 — must pass CPU F32 comparison first:
- SIMD vectorization;
- FP16 intermediates outside explicit Preview;
- alternate/custom mip generation.

No approximation is accepted in Auto/Final solely for speed.
