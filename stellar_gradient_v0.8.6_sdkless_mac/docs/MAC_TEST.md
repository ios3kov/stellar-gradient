# macOS Validation — Stellar Gradient v0.10.0

Current gate: real After Effects **Level-2 Validation** of the unchanged artifact.

## Exact artifact

- native commit: `9f3e73bc92534941db1106f521786d8c3c792347`
- Mac CI: `36911997718` — 8/8 PASS
- Actions artifact: **11187002476**
- Build ID: `sg-0.10.0-9f3e73bc9253-clean-bc5efe6bf48a-aarch64-apple-darwin-36911997718.1`
- plugin ZIP SHA-256: `4195641fe2b2a79e528cd1a80b3a10f33b278b3789fface7d71a80370a0e1c30`

Do not use `FIRST_MAC_BUILD.command` for this gate: rebuilding would create a
different candidate.

## 1. Identity

Run `diagnostics/Stellar_Mac_Check.command`.
The intended bundle must report `TARGET ON-DISK IDENTITY: MATCH`.

Then open Stellar Gradient's About message in After Effects and record the loaded
Build ID. It must exactly match the target above. Multiple/ambiguous Stellar
bundles or a wrong Build ID are **BLOCKED**, not PASS.

## 2. Automated host smoke

Run `diagnostics/Stellar_AE_Diagnostics_v1_3.jsx` via
**File > Scripts > Run Script File**.

It requests CPU at 8/16/32 bpc and Auto at 32 bpc. It proves host completion and
cleanup only; it does not prove numerical pixels or actual GPU dispatch.

## 3. Manual Level-2 matrix

On the same exact artifact:
- Render Engine GPU with Metal, then CPU; compare numerically where evidence permits.
- Exercise 8/16/32 bpc, transparent pixels, alpha/HDR/extended-range and color management.
- Check Base/Depth/Turbulence Size 3 & 6/Softness/Grain/Glow/Diffusion.
- Scrub and render multiple frames with MFR enabled.
- Check Undo/Redo, duplicate/copy, save/reopen, AE restart and Render Queue.
- Only after correctness passes, record comparable real-host CPU/Metal timings.

Any crash, host error, stale identity, seam or reproducible parity difference
blocks acceptance. No merge/release follows from smoke alone.
