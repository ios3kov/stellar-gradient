# Status — v0.8 SDK-less Mac candidate

Date: 2026-09-26

## Plan status

1. **SDK-less layer audit — COMPLETE**
   - Found release panic-boundary risk and GPU-data ownership risk.
   - FFI layout and parameter-ID contracts reviewed.
   - See `SDKLESS_AUDIT_V08.md`.

2. **Fixes — COMPLETE**
   - Release panic catching enabled.
   - GPU context changed to single-owner setup/setdown lifetime with immutable concurrent render access.
   - C++/Rust ABI size/alignment/offset guards added.
   - Public parameter IDs pinned 1..49 on `#[repr(i32)] Params`.
   - Rust quality gates added before packaging.
   - See `SDKLESS_FIXES_V08.md`.

3. **Final code-side regression — COMPLETE**
   - Core strict: 7/7 PASS.
   - Core ASan/UBSan: 7/7 PASS.
   - Core TSan: 7/7 PASS.
   - Bridge parity: `max_err=0`.
   - Bridge HDR/8/16/32 checks: PASS.
   - Bridge MFR: `max_err=0`, failures=0.
   - Bridge ASan/UBSan + TSan: PASS.
   - Mac-only Rust compiler gates are embedded into `FIRST_MAC_BUILD.command` and execute before packaging.
   - See `REGRESSION_V08.md`.

4. **v0.8 source freeze — COMPLETE**
   - Host/bridge version: v0.8.6 build 14 (MFR/cfg compiler hotfix; render core unchanged).
   - Render core remains frozen v0.6 and unchanged.
   - 49 source/build inputs frozen by SHA-256.
   - `python3 tools/verify_v08_host_freeze.py` => PASS.
   - See `SOURCE_FREEZE_V08.md`.

5. **Native Mac `.plugin` build — RETRY READY / CURRENT GATE**
   - First v0.8 Mac run on 2026-09-26 stopped before compilation: Homebrew `cargo/rustc 1.80.1` existed, but `rustup` did not; the script incorrectly called `rustup` unconditionally.
   - Root cause: Rust 1.80.1 is also too old for the host's Rust 2024 edition (minimum 1.85).
   - Fixed in v0.8.1 build 9: modern complete Rust is used directly; old/incomplete Rust triggers automatic user-local stable rustup bootstrap.
   - Second Mac run reached sanitizer smoke and stopped because macOS ASan rejects `detect_leaks=1`; this was a preflight-script portability defect, not a plugin/core failure.
   - Fixed in v0.8.2 build 10: macOS sanitizer smoke uses `detect_leaks=0` with ASan/UBSan halt-on-error enabled.
   - Adobe SDK is not required.
   - Third Mac run reached the Rust host gate after contracts, bridge parity and sanitizer smoke passed. It exposed invalid Rust leading-dot floats (`.13`, `.01`, etc.) and rustfmt style drift.
   - Fixed in v0.8.3 build 11: all literals use `0.xx`, a static syntax contract prevents recurrence, and rustfmt/check/test/clippy run on an ephemeral build copy so the frozen source bytes are not modified.
   - Retry the v0.8.6 `FIRST_MAC_BUILD.command` on the Apple Silicon test Mac.
   - It performs Rust fmt/check/test/clippy, native bridge checks, Release arm64 build, bundle/codesign validation, install, diagnostics and AE launch.

6. **AE functional/quality test — BLOCKED BY 5**
7. **Metal profiling — BLOCKED BY 6**
8. **Profiler-driven optimization — BLOCKED BY 7**
9. **Cosmic vs Stellar controlled benchmark — BLOCKED BY 8**
10. **Final user test `.plugin` — BLOCKED BY 9**

## Release rule

Do not claim production readiness or speed superiority over Cosmic until points 5–9 have passed on the same Mac/After Effects environment.


### v0.8.5 Mac compile hotfix

The fourth real Mac run reached Objective-C++ Metal compilation and exposed Xcode's macOS 15+ deprecation of `MTLCompileOptions.fastMathEnabled`. The SDK-less bridge now uses safe/precise modern Metal compile options with a guarded legacy fallback. Render core and shader math are unchanged.


## macOS build retry — v0.8.6 build 14

The v0.8.4 run reached native Rust host compilation. Metal compiled past the precise-math gate, but Rust 1.98 exposed that `threaded_rendering` was not active in the destination crate even though the PiPL advertised MFR. The `after-effects` macro therefore generated a mutable `handle_command(&mut self, ...)` trait while the MFR-safe host implemented `&self`.

v0.8.5 registers all macro cfg names explicitly and pins `threaded_rendering`, `smart_render`, and `gpu_render` in this crate's build script. This keeps the Rust host trait and the advertised PiPL capabilities in one deterministic contract. Two `unused_mut` warnings are also removed so the later `clippy -D warnings` gate remains strict. Frozen C++ render core and Metal shader math are unchanged.


### GitHub CI migration — 2026-09-27

- Full v0.8.6 source tree is now in `ios3kov/stellar-gradient` under `stellar_gradient_v0.8.6_sdkless_mac/`.
- The first three GitHub Actions runs used the earlier temporary minimal archive; native C++/Metal passed, but Rust still failed with E0053 because crates.io `after-effects 0.4.0` generated the non-MFR `&mut self` trait.
- Upstream inspection confirmed current MFR examples use `&self`. The SDK-less host now pins both `after-effects` and `pipl` to exact upstream revision `83dcc93734fd5db1335b6ec83cba7a6505a39dcc` so the macro/API implementation is deterministic.
- Frozen render core and Metal shader math are unchanged.
- CI now runs independent contracts, core strict/ASan/TSan, native bridge/Metal, Rust quality, and ARM64 bundle jobs in parallel.


### CI tree recovery — 2026-09-27

- Run #4 failed before meaningful code tests because the CI hotfix tree was accidentally created without the uploaded source tree as its Git base; this appeared as missing CMake/Rust/bridge files.
- The source upload commit `b6fa596b...` is now the canonical base tree and all uploaded project files are restored in a normal fast-forward commit.
- The exact upstream MFR dependency pin and parallel CI workflow are retained.
- `verify_sdkless_host.py` now validates the pinned upstream dependency instead of requiring the old crates.io strings.
- Render core and Metal shader math remain unchanged.


### GitHub Mac CI run #5 — 2026-09-27

- Static contracts: PASS.
- Core strict: PASS.
- Core ASan/UBSan: PASS.
- Core TSan: PASS.
- Native bridge + Metal: PASS, including bridge ASan/UBSan.
- ARM64 plugin bundle: PASS. The SDK-less Rust host compiled against the pinned upstream revision, the macOS bundle was created, ad-hoc signed, verified, and uploaded as the `StellarGradient-mac-arm64` artifact.
- The previous MFR E0053 host ABI blocker is resolved.
- Rust host quality was the only red job and stopped at `cargo fmt --check` before compile checks. This is CI-policy drift: the frozen canonical source is intentionally unformatted in places, while the local Mac gate formats an ephemeral copy. CI is now aligned with that policy by formatting and testing `.sdkless-ci`.
- Render core and Metal shader math remain unchanged.

Plan item 5: **COMPLETE — real ARM64 .plugin built in CI**.
Plan item 6: **NEXT — After Effects functional/quality validation** after the all-green CI confirmation.


### Rust quality cleanup — 2026-09-27

- GitHub Mac CI run #6: 6/7 jobs passed, including repeated ARM64 plugin build/bundle/sign/upload.
- Rust `cargo check --release` and the FFI layout unit test passed against the pinned upstream dependency.
- The only remaining failure was Clippy `needless_range_loop` in the palette-copy loop.
- The loop now uses `iter_mut().zip(...iter())`; output values and render behavior are unchanged.
- Strict `clippy -D warnings` remains enabled; no lint suppression was added.
- SHA-freeze updated only for `sdkless/src/lib.rs`.


### GitHub Mac CI run #7 — ALL GREEN

Commit: `6894bb0738b0e58345a102bf2dbdd5686c87d9db`

- Static contracts: PASS.
- Core strict: PASS.
- Core ASan/UBSan: PASS.
- Core TSan: PASS.
- Native bridge + Metal: PASS.
- Rust host quality (fmt/check/test/clippy -D warnings): PASS.
- ARM64 plugin bundle/sign/upload: PASS.

Plan item 5 is fully complete. Proceed to item 6: install the CI artifact on the target Apple Silicon Mac and perform After Effects functional/quality validation.


### After Effects manual smoke — 2026-09-27

User installed the CI-built ARM64 `StellarGradient.plugin` on the target Apple Silicon Mac and reported that it appears to work in After Effects.

Plan item 6: **PROVISIONAL PASS — plugin loads and renders in AE**.

Before marking item 6 fully complete, the remaining manual quality checks are: key parameter response, GPU/CPU engine switching, 8/16/32-bpc/HDR behavior, and basic scrub/render stability. No production-speed claim yet.


### Palette presets — v0.8.7 build 15

- Reworked the top color UX to match Cosmic's documented structure: `Palette` group, `Colors` preset menu, then five editable color controls.
- Selecting a curated preset writes the preset into Color 1..5, so the user sees and can edit the actual colors.
- Manually editing any of Color 1..5 automatically returns the `Colors` menu to `Custom`.
- Added 15 curated palettes plus Custom.
- Existing persistent parameter IDs 1..49 remain unchanged; new UI-only Palette group markers use IDs 50 and 51.
- Render core and Metal shader math are unchanged.
- CI push filtering now avoids full rebuilds for ordinary docs-only commits.


### Palette preset CI cleanup

- Rust check and FFI layout unit test passed for v0.8.7.
- Removed one redundant `.into_iter()` flagged by strict Clippy.
- No behavioral/render change.


### Original-style control correction — v0.8.8 build 16

UI/host corrections based on observable Cosmic behavior:

- `Colors` renamed to `Presets` as requested.
- Preset list now uses the ten visible Cosmic preset names: Retro Pop, Sage, Blush, Deep Space, Electric, Ultraviolet, Lagoon, Sunset, Pride Rainbow, Candy. Palette color values remain our independent clean-room values.
- `Angle` is now a native AE Angle control, default 90°.
- `Phase` is also a native AE Angle control, default 0°.
- `Offset` is a percentage control: valid/slider −100..100%, default 0%; host maps it to the core's normalized offset.
- Depth UI now matches the observable original ranges/defaults:
  - Contrast 0..400%, slider 0..200%, default 100%.
  - Bulge −200..200%, slider −100..100%, default 60%.
  - Rounding 0..100%, default 100%.
- Host maps Depth percentages to the existing clean-room core as normalized values (Contrast/Bulge/Rounding divided by 100).
- Depth group is open by default.
- Frozen C++ render core and Metal shader math are unchanged.


### GitHub Mac CI run #12 — ALL GREEN

Commit: `1fa06bfc2f45b677ba3006f7c5f3f658dd43ec73`

- Static contracts: PASS.
- Core strict: PASS.
- Core ASan/UBSan: PASS.
- Core TSan: PASS.
- Native bridge + Metal: PASS.
- Rust host quality: PASS.
- ARM64 plugin bundle/sign/upload: PASS.

The v0.8.8 UI corrections are ready for manual After Effects verification.


### Cosmic palette parity — v0.8.9 build 17

- Compared the supplied Cosmic.aex binary directly against the Stellar host.
- Original built-in Colors menu is: Cold, Retro Pop, Sage, Blush, Deep Space, Electric, Ultraviolet, Lagoon, Sunset, Pride Rainbow, Candy, separator, Custom.
- Deep Space is the original default selection.
- Stellar now uses the exact observable five editable RGB values for all 11 built-in palettes.
- The original UserChangedParam handler changes only Color 1..5 when a built-in palette is selected; it does not overwrite Angle, Cycles, Offset, Phase, Depth, Turbulence, Look, Grain or Optical Diffusion. Stellar preserves that behavior.
- Manual color edits switch the menu to Custom.
- Palette remains expanded by default; Depth, Turbulence, Look and all nested Look groups are collapsed by default.
- Render core and Metal shader math are unchanged.


### Cosmic full control defaults — v0.9.0

- Re-disassembled the supplied Cosmic.aex down to its PARAMS_SETUP and USER_CHANGED_PARAM paths.
- Corrected Stellar's shared non-color default state and UI ranges to the values encoded by the supplied binary.
- Saturation/Brightness now use Cosmic-style percentage controls and host normalization.
- Turbulence now starts at Amount 40, Size X/Y 3.0, Evolution 0°, Softness 40; Evolution is a native AE Angle control.
- Glow now starts at Radius 194, Falloff 50%, Threshold 0%, Intensity 160%, Soft Clip 0%.
- Grain now starts at Amount 20%, Size 1.0, Color 100%, Animate On.
- Optical Diffusion now starts at Blur 15, Center 50/50, Focus 50, Feather 450, Invert Off.
- Depth remains Contrast 100%, Bulge 60%, Rounding 100%; Palette is the only expanded group by default.
- The supplied Cosmic.aex built-in Colors handler writes Color 1..5; the non-color settings above are its shared effect defaults, so matching both pieces is required for visual preset parity.


### Full preset state — v0.9.1

- Built-in Presets now apply the full recovered Cosmic effect state, not only Color 1..5.
- Selecting a preset restores Angle/Cycles/Offset/Phase, Saturation/Brightness, Depth, Turbulence, Glow, Grain and Optical Diffusion values to the Cosmic defaults recovered from the supplied binary.
- Editing any of those effect controls switches Presets to Custom.
- Render Engine and Quality stay Stellar-specific and do not participate in preset identity.


### Palette structure parity

- Moved Palette group end to the original Cosmic position after Brightness.
- Palette now contains Presets, Color 1..5, Angle, Cycles, Offset, Phase, Saturation and Brightness.
- Depth remains a separate collapsed group.
- Updated the static verifier for the full-preset implementation and the exact Palette group ordering.


### Native Optical Diffusion Center — v0.9.2

- Replaced the visible Center X / Center Y sliders with one native AE Point control named Center, matching Cosmic.
- Center defaults to 50/50 and is normalized against the source layer dimensions for the render core.
- Legacy Center X/Y IDs 41/42 remain present but invisible; the new Point uses ID 52.
- Full preset application resets Center to the actual layer center.


### Cosmic directional Depth — v0.9.3

- Extracted the CUDA PTX embedded in the supplied Cosmic.aex and recovered CosmicDepthKernel.
- Added native Depth > Angle (ID 53, default 0°) without renumbering existing parameters.
- Replaced the old radial dome with the recovered directional depth ramp on CPU and Metal: Angle -> cos/sin, Contrast shapes the mask, Bulge offsets the gradient coordinate.
- CPU and Metal use the same equation.
- Rounding is currently a fused continuous smoothing approximation. Cosmic applies it as a separate blur of the depth map; exact Rounding blur parity remains the next Depth item.


### Exact Cosmic Depth Rounding — CPU stage

- Recovered the Rounding path from the supplied Cosmic.aex.
- Depth now builds a separate alpha-weighted directional map before colorization.
- Rounding sigma: min(width,height) * 0.125 * 0.7 * clamp(Rounding,0..4).
- If sigma > 0.5, radius = max(1, round(sigma/3)); exactly three horizontal+vertical box-blur pairs are applied.
- Contrast is applied after the rounded depth map, then Bulge offsets the gradient coordinate.
- CPU implementation landed first so core golden signatures can validate the recovered algorithm before the same pass graph is added to Metal.


### Exact Cosmic Rounding — CPU + Metal

- Verified against user-supplied Cosmic.aex SHA-256 b1b55fc6f0795dad45dfd1e79bfd180eb6d881519aade4250bd6416d737a3ea8.
- Exact host formula: R = min(width,height) * 0.125 * 0.7 * Rounding.
- For R >= 0.5: pass radius = max(1,lroundf(R/3)); exactly three H/V box-blur pairs.
- DepthKernel precedes this blur path, so directional depth + Contrast are generated first; Rounding blurs that depth map; Bulge is applied during colorization.
- CPU keeps the MFR-safe sequential box implementation.
- Metal uses a separate R32F depth texture and three MPSImageBox passes with clamp edges and the identical odd kernel diameter.


### Cosmic palette behavior correction — v0.9.4

- Re-verified USER_CHANGED_PARAM in the user-supplied Cosmic.aex.
- Built-in palette selection changes only Color 1..5.
- Removed Stellar's incorrect behavior that reset Angle/Cycles/Offset/Phase, Depth, Turbulence, Glow, Grain and Optical Diffusion whenever a palette was selected.
- Non-color parameters now remain untouched when switching palettes, matching the original binary.
- Manual Color 1..5 edits still switch the menu to Custom.
