# v0.5 hardening audit

## Closed in code

- MFR/thread safety: shared worker pool is synchronized; per-render CPU scratch remains `thread_local`; Metal device data contains immutable pipeline states only.
- ThreadSanitizer: 8 concurrent heavy renders produce identical output (`max_err=0`) with no reported data races.
- NaN/Inf/extreme parameters: all public parameters are sanitized to their AE valid ranges before CPU/GPU math.
- Alpha: base rendering remains premultiplied; transparent base pixels contain zero RGB; glow is allowed to reveal zero alpha by design.
- HDR/32 bpc: CPU F32 test confirms RGB values above 1.0 are preserved; only alpha is constrained to [0,1].
- Memory pressure: glow and diffusion reuse one CPU mip pyramid instead of retaining two. Large/MFR scratch is trimmed after render. 8K pyramid size arithmetic is checked for overflow.
- Allocation failures: `std::bad_alloc` maps to `PF_Err_OUT_OF_MEMORY`.
- GPU setup fallback: Metal device/pipeline setup can cleanly decline GPU support; AE can then call CPU SmartRender.
- Compiler math: no C++ fast-math flags; Metal uses `fastMathEnabled=NO`; Auto/Final keep FP32 intermediates.

## Regression result

- strict build (`-Werror`): PASS
- CPU golden signatures: PASS (unchanged)
- tile consistency: PASS
- NaN/Inf + HDR/premult + 8K planning: PASS
- MFR 8-way determinism: PASS (`max_err=0`)
- ASan + UBSan: PASS
- ThreadSanitizer: PASS
- total tests: 7/7 PASS

## Must still be verified on the real Mac host

- actual Objective-C++/Adobe SDK compile
- actual Metal shader compile on Apple Silicon
- AE plug-in loading and SmartFX host behavior
- Metal runtime errors / memory pressure at 4K and 8K
- CPU-vs-GPU image parity from rendered frames
- Instruments / Metal System Trace timings
- Cosmic vs Stellar benchmark

No speed claim against Cosmic is valid until those host measurements are completed.
