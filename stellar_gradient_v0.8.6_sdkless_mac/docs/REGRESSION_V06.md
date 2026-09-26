# Regression v0.6

Date: 2026-09-26
Plan point 3/10: COMPLETE.

## Results

- metadata contract: PASS
- Metal performance/quality source contract: PASS
- AE SmartFX/MFR/cache/pixel-format source contract: PASS
- code-freeze source contract: PASS
- Release strict build (`-Werror`): PASS
- Release tests: 7/7 PASS
- ASan + UBSan: 7/7 PASS
- ThreadSanitizer: 7/7 PASS
- MFR stress: PASS, deterministic
- CPU F32 golden signatures: PASS, unchanged

## CPU reference benchmark (current environment)

- 640×360 base: 4.872 ms/frame
- 640×360 procedural: 16.958 ms/frame
- 640×360 full: 56.096 ms/frame
- 1280×720 base: 12.978 ms/frame
- 1280×720 procedural: 54.591 ms/frame
- 1280×720 full: 176.825 ms/frame

These CPU numbers are regression telemetry, not a prediction of Apple Silicon Metal performance.
