# SDK-less Host Audit v0.8

Date: 2026-09-26
Scope: Rust AE host, C ABI bridge, Metal bridge, parameter ABI/IDs, release build and install path. Frozen render core v0.6 is out of scope unless a correctness defect requires a change.

## Status

Audit complete. All identified pre-Mac correctness fixes have been implemented; see `SDKLESS_FIXES_V08.md`.

## Findings

1. **Release panic boundary — FIXED (high)**
   - `after-effects` catches panics in debug builds, but optimized release builds only catch them when `cfg(catch_panics)` is enabled.
   - `Cargo.toml` correctly uses `panic = "unwind"`, but `build.rs` does not currently enable `catch_panics`.
   - Risk: a Rust panic in release could cross the C ABI boundary into After Effects.

2. **GPU data access under concurrent GPU renders — FIXED (high)**
   - The host currently uses the binding helper `SmartRenderExtra::gpu_data::<T>()`.
   - That helper reconstructs a `Box` from the raw GPU-data pointer before leaking it again. Reconstructing ownership is undesirable for data that can be read by concurrent GPU render calls.
   - Plan: own `GpuContext` directly with `Box<GpuContext>` in the plugin and use read-only raw-pointer access during render; reclaim exactly once in `GPU_DEVICE_SETDOWN`.

3. **FFI layout guard — FIXED (medium)**
   - C and Rust structs are both `repr(C)`/plain C layout and currently match, but the contract is not enforced on both sides.
   - Current measured C++ layout: `SGParamsC = 180 bytes`, `SGRenderStateC = 264 bytes`, `time_seconds offset = 248`, `engine_mode offset = 260`.
   - Plan: add C++ `static_assert`s and Rust unit tests for size/alignment/offsets.

4. **Parameter IDs/order — PASS + AUTOMATED CONTRACT**
   - IDs 1..49 are explicitly assigned, including group markers.
   - No dynamic ID generation is used for the public parameters.
   - Plan: verifier will enforce uniqueness, contiguous IDs and exact Engine/Quality IDs.

5. **Fast-math / final quality — PASS**
   - C++ bridge uses `-fno-fast-math -ffp-contract=off`.
   - Metal library uses `fastMathEnabled = NO`.
   - Rust does not request unsafe FP approximations.
   - Build script will clear user-provided `RUSTFLAGS`/`CARGO_ENCODED_RUSTFLAGS` for the release candidate.

6. **GPU BGRA128 channel contract — PASS**
   - Metal explicitly swizzles AE `BGRA128` worlds to internal RGBA and back.

7. **GPU setup fallback — PASS with documented limit**
   - Unsupported/non-Metal devices or failed Metal context creation do not advertise GPU support, so AE can choose CPU SmartRender.
   - A runtime Metal failure after GPU render has already started cannot be converted to CPU inside the same selector because input/output are GPU worlds; that case returns a host error and will be investigated only if reproduced in AE.

8. **Build/sign/install script — PASS with strengthening required**
   - No Adobe SDK dependency.
   - arm64 bundle, local ad-hoc codesign and MediaCore install are present.
   - Plan: add `cargo fmt --check`, `cargo check`, `cargo test`, `cargo clippy -D warnings` before release packaging and preserve diagnostics on failure.

## Gate

Do not declare point 1 complete until the findings are documented (this file) and point 2 fixes are implemented without changing frozen core output.
