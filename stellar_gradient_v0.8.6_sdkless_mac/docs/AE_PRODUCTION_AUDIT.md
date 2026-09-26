# After Effects production audit

## Verified in source/tests

- SmartFX pre-render and Smart Render paths are used; legacy render is intentionally rejected.
- MFR flag is enabled and CPU scratch is thread-local; immutable Metal pipelines are device-lifetime objects.
- Animated grain is correctly marked `NON_PARAM_VARY` so AE cannot reuse a still-frame cache across time. Dynamic flags clear it when animated grain is inactive to recover caching performance.
- CPU handles ARGB32, ARGB64 and ARGB128; GPU accepts AE's BGRA128 GPU world.
- Output is premultiplied: base RGB is multiplied by alpha, Glow/Diffusion operate on premultiplied RGBA, alpha is clamped to [0,1]. Float RGB remains HDR-capable.
- Glow/Diffusion use an explicit SmartFX working halo and mip alignment. The host-facing result remains clipped to the requested output tile.
- Procedural Turbulence/Grain coordinates use layer-space origin so they do not restart at tile boundaries.
- CPU/GPU quality plan is shared and Auto/Final keep FP32 intermediates.
- CPU F32 golden signatures and full-frame-vs-tile tests are locked.

## Still requires a real Mac/After Effects host

The Linux audit environment cannot compile Metal or load Adobe After Effects. The following gates are therefore mandatory on the user's Mac before a test plug-in is called release-ready:

1. compile against the installed/current AE SDK;
2. AE loads the bundle without plug-in errors;
3. Effect Controls parameters render correctly at 8/16/32 bpc;
4. CPU/GPU/Auto switch works and CPU fallback is automatic when GPU is unavailable;
5. MFR stress render (multiple frames) shows no races or seams;
6. 1080p/4K tiled renders match full frame;
7. Metal readback passes the CPU F32 quality contract;
8. Instruments/Metal System Trace confirms no unexpected CPU/GPU synchronization or allocation spikes.
