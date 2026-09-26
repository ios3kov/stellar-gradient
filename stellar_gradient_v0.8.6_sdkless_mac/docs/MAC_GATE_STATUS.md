# Native Mac gate — v0.8

Date: 2026-09-26
Plan point 5/10: **RETRY READY / CURRENT GATE**.

## Architecture

- Frozen render core: v0.6, unchanged and SHA-verified.
- AE host/PiPL: v0.8.6 build 14 SDK-less Rust (`after-effects` 0.4 / `pipl` 0.1), using pre-generated AE ABI bindings.
- CPU path: frozen C++ FP32 reference renderer through a thin C ABI bridge.
- GPU path: frozen Metal shader/core through an Objective-C++ bridge using AE-supplied MTLDevice / MTLCommandQueue / GPU-world MTLBuffer pointers.
- Auto/Final: FP32 intermediates; Preview may use FP16.
- Metal fast-math: disabled.
- Adobe After Effects SDK: **not required**.

## Verified before Mac run

- v0.8 SHA source freeze: PASS (49 files).
- core strict / ASan+UBSan / TSan: 7/7 PASS each.
- bridge parity: `max_err=0`.
- bridge HDR/8/16/32: PASS.
- bridge MFR: `max_err=0`, failures=0.
- bridge ASan/UBSan + TSan: PASS.
- release panic boundary: hardened.
- stable AE disk IDs: 1..49.

## One action on the Mac

Use the **v0.8.6 package**, then double-click:

`FIRST_MAC_BUILD.command`

It will:

1. verify macOS / Apple Silicon / Xcode tools;
2. accept an existing Rust toolchain only when `rustc >= 1.85` and `rustfmt` + `clippy` are available;
3. otherwise automatically install/update a user-local stable Rust via official rustup, even when an older Homebrew Cargo already exists;
4. verify source freezes/contracts and bridge parity;
5. run bridge sanitizer smoke;
6. run Rust `fmt/check/test/clippy` quality gates;
7. build the SDK-less arm64 Release host;
8. create/sign/verify `StellarGradient.plugin`;
9. install it to the user MediaCore plug-in folder;
10. capture environment/build diagnostics and launch After Effects.

## After success

Proceed immediately to point 6: `docs/MAC_TEST.md`.

## First real Mac attempt — 2026-09-26

Observed before any plugin compilation:

```text
cargo 1.80.1
rustc 1.80.1
command not found: rustup
```

This was a build-script bootstrap defect, not a render-core, AE, or Metal failure. Rust 1.80.1 is below the minimum required for edition 2024. v0.8.1 build 9 fixes the bootstrap and requires no manual Rust commands from the tester.

## Second real Mac attempt — 2026-09-26

Observed after Rust bootstrap and all source/bridge contracts passed:

```text
[1/8] Frozen-core + SDK-less contracts ... PASS
[2/8] Native bridge parity
bridge parity max_err=0
[3/8] Sanitizer smoke
AddressSanitizer: detect_leaks is not supported on this platform.
```

This is a macOS sanitizer-option incompatibility. The bridge executable itself did not report an address or undefined-behavior failure. v0.8.2 build 10 disables unsupported LeakSanitizer detection on macOS while preserving ASan/UBSan halt-on-error behavior. Leak/resource-lifetime validation moves to the later macOS-native AE/Metal profiling gate.

## Third real Mac attempt — 2026-09-26

Observed after bootstrap, source contracts, native bridge parity and sanitizer smoke all passed:

```text
[1/8] Frozen-core + SDK-less contracts ... PASS
[2/8] Native bridge parity ... max_err=0
[3/8] Sanitizer smoke ... PASS
[4/8] Rust host quality gates
error: float literals must have an integer part
```

The failing tokens were host-only Rust literals such as `.13`, `.5`, and `.01`; render-core/Metal code was not involved. v0.8.3 build 11 normalizes them to `0.xx`, adds a static no-leading-dot-floats contract, and performs rustfmt in an ephemeral `.sdkless-build` copy so the SHA-frozen canonical source is not rewritten.


## Fourth real Mac attempt — 2026-09-26

Observed after source contracts, bridge parity, sanitizer smoke, Rust dependency resolution and host compilation began:

```text
MetalBridge.mm: error: 'fastMathEnabled' is deprecated: first deprecated in macOS 15.0 - Use mathMode instead [-Werror,-Wdeprecated-declarations]
```

This is a current-Xcode API compatibility failure in the SDK-less Metal bridge, not a shader/core/math failure. v0.8.4 build 12 replaces the deprecated setting with `MTLMathModeSafe` + `MTLMathFloatingPointFunctionsPrecise` on macOS 15+ while retaining a compile/runtime-guarded legacy fallback. `-Werror` remains enabled and the no-fast-math quality contract is unchanged.

## Fifth Mac run — Rust MFR cfg mismatch fixed

The v0.8.4 run passed contracts, bridge parity, sanitizer smoke and the modern Metal compile-option gate, then reached real Rust host compilation. Rust reported `E0053`: the generated `AdobePluginGlobal::handle_command` expected `&mut self` because `threaded_rendering` was not active in the destination crate, while the host implements `&self` for MFR-safe shared global state.

v0.8.6 build 14 explicitly registers/pins the macro cfgs (`threaded_rendering`, `smart_render`, `gpu_render`, panic and compatibility cfg names) in this crate's build script. The PiPL still advertises MFR/GPU support, so compile-time trait shape and host capability flags are now one contract. Two `unused_mut` warnings were removed for the strict clippy gate.
