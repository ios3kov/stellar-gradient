# Next steps — v0.8

## Current gate: real Mac build

1. Use the v0.8 source-freeze package.
2. Double-click `FIRST_MAC_BUILD.command`.
3. Do not install or download the Adobe After Effects SDK; it is not needed.
4. If the build fails, return `mac_first_build_report.zip` and the terminal output.
5. If the build succeeds, continue directly with `MAC_TEST.md`.

## After point 5 succeeds

6. Run AE functional/quality matrix: 8/16/32 bpc, Linear/HDR, animation, MFR, CPU/GPU, 4K/8K.
7. Run Metal profiling: GPU time, allocations, bandwidth, dispatch count, memory pressure.
8. Optimize only measured bottlenecks; preserve the FP32 quality contract.
9. Run controlled same-Mac Cosmic vs Stellar benchmark.
10. Package the final user test `.plugin` only after all gates pass.
