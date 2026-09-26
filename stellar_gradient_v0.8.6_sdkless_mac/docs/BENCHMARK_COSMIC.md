# Stellar Gradient vs Cosmic — benchmark protocol

## What can be established before the Mac host test

The supplied Windows Cosmic sample exposes 21 unique CUDA kernel symbols, including separate depth, H/V box blur, glow source/downsample/normalize/upsample/combine, SAT row/column + variable blur, and composite stages. This is evidence of a multi-stage GPU pipeline, but kernel-symbol count is **not** a performance result and the macOS Metal implementation may differ.

The current Stellar heavy Metal path is intentionally smaller:

- Base + Glow seed: 1 fused compute pass
- Glow mip generation: 1 blit/mip stage
- Glow composite into base: 1 compute pass when Diffusion is enabled
- Diffusion mip generation: 1 blit/mip stage
- Diffusion + output: 1 compute pass

Disabled modules remove their stages; base-only is a direct single compute pass into the AE GPU world.

## Fair Mac test

Use the same Mac, AE version, project bit depth, comp, source layer, frame range and render cache state. Warm each case before measurement.

Primary matrix:

- 1920x1080 and 3840x2160
- 32 bpc
- 100 measured frames after 20 warm-up frames
- Base only
- Depth + Turbulence
- Glow small / medium / large
- Diffusion small / medium / large
- Heavy: Depth + Turbulence + Grain + Glow + Diffusion
- Stellar Auto/GPU and CPU fallback
- Cosmic GPU and CPU/fallback where supported

Record:

- wall-clock median ms/frame
- p95 ms/frame
- Metal GPU duration where available
- peak memory
- first-frame/cold-start separately from warm render
- visible result screenshots and 32-bit EXR output

Do not compare a low-quality Stellar Preview against a high-quality Cosmic render. Auto/Final are FP32 and are the production comparison modes.

## Quality gate

Stellar is not allowed to win by reducing quality. Before accepting a faster path, the same build must pass `docs/QUALITY_CONTRACT.md` and the Mac full-frame/tile tests.

## One-command environment capture

Run `./tools/mac_benchmark_env.sh` and keep its output with the benchmark results. This records the Mac/Metal/AE/plugin environment so timings are reproducible.
