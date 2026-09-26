# SDK-less Mac host — v0.7

Date: 2026-09-26

## Why this exists

The first v0.6 Mac build script incorrectly treated the Adobe After Effects C++ SDK as a local prerequisite. The render core never actually needs to be coupled to those source headers.

The v0.7 host separates responsibilities:

- `after-effects` / `pipl` Rust crates: AE ABI, command selectors, PiPL and parameter plumbing;
- frozen v0.6 C++ core: CPU reference math and render planning;
- Objective-C++ bridge: native Metal dispatch only;
- AE-owned GPU worlds remain zero-copy MTLBuffer inputs/outputs.

## Quality/performance rules retained

- Auto/Final use FP32 intermediates.
- Preview alone may use FP16 intermediates.
- Metal shader compilation explicitly disables fast math.
- CPU and GPU share the same sanitized parameter model and render plan.
- No CPU/GPU wait is introduced after Metal command-buffer commit.
- Unsupported Metal setup declines GPU support and lets AE use CPU SmartRender.

## Verification before first Mac build

- Frozen core SHA contract: PASS.
- Strict regression: 7/7 PASS.
- ASan/UBSan: 7/7 PASS.
- TSan/MFR: 7/7 PASS.
- C ABI bridge parity vs direct CPU reference: max_err = 0.
- SDK-less source contract: PASS.

## Remaining gate

The Rust/Metal host cannot be compiled in the current Linux environment because the final binary requires Apple's Metal/Foundation frameworks. The first real compile/load therefore occurs on the user's Apple Silicon Mac via `FIRST_MAC_BUILD.command`.
