# Stellar AE diagnostics — v0.10 Validation handoff

This is a **Validation** handoff, not a product release.

Exact native target:
- commit: `9f3e73bc92534941db1106f521786d8c3c792347`
- Mac CI: `36911997718` — 8/8 jobs PASS
- Actions artifact: **11187002476**
- Build ID: `sg-0.10.0-9f3e73bc9253-clean-bc5efe6bf48a-aarch64-apple-darwin-36911997718.1`
- plugin ZIP SHA-256: `4195641fe2b2a79e528cd1a80b3a10f33b278b3789fface7d71a80370a0e1c30`

Canonical target metadata: `validation_target_v010.json`.
The active handoff tools are pinned to this identity by CI.

## Order of use

1. Do **not** rebuild a substitute candidate.
2. Run `Stellar_Mac_Check.command` against the intended installed plugin.
   The intended bundle must report `TARGET ON-DISK IDENTITY: MATCH`.
3. In After Effects, record the effect's About Build ID. It must equal the target above.
4. Run **File > Scripts > Run Script File** and select
   `Stellar_AE_Diagnostics_v1_3.jsx`.
5. Keep the fresh `Desktop/Stellar-AE-*/` report/events/captures as evidence.

A static disk match does not prove which binary AE actually loaded.

## What v1.3 checks

v1.3 preserves the v1.2 deferred eight-case host smoke:
- registration and effect initialization;
- repeated default-tree stability;
- CPU requests at 8/16/32 bpc;
- Auto request at 32 bpc;
- render-queue completion and capture integrity;
- strict cleanup and project-safety guards.

`SMOKE_ONLY_PASS` does **not** prove pixel parity, HDR/alpha correctness,
actual GPU execution, lifecycle parity, performance, or complete Cosmic parity.

## Remaining Level-2 evidence

The current Reference Audit is PARTIAL. The exact candidate still needs real-AE:
CPU/GPU numerical parity, 8/16/32 bpc + alpha/HDR/color management,
restart/Undo/Redo/save-reopen/migration, MFR/render queue/noninteractive paths,
Glow/Diffusion parity, and real-host profiling.

Historical v0.9.6 diagnostics remain historical evidence only. They must not
certify the current v0.10 artifact.
