# Diagnostic 1.3 — exact v0.10 Validation target

Date: 2026-10-01.

Current process:
- AE Development Rules v4.0.0 commit
  `58d14aa12375757f4e52396d951f1557ae4f453c`;
- AI Smart Entry blob
  `29cb44eb7f2a4e0f7c120a97ae434dbb7e58174e`;
- whole-product Cosmic Reference Audit: **PARTIAL**;
- Delivery Gate: **Validation**.

## Scope

v1.3 preserves v1.2 preflight, deferred-render and cleanup behavior. It changes
the active handoff identity to exact native commit `9f3e73bc92534941db1106f521786d8c3c792347`,
artifact **11187002476**, Build ID `sg-0.10.0-9f3e73bc9253-clean-bc5efe6bf48a-aarch64-apple-darwin-36911997718.1`.

It does not install/rebuild the plugin and does not infer runtime identity from
registration. `Stellar_Mac_Check.command` independently checks on-disk identity;
the loaded About Build ID remains real-AE evidence.

The active Cosmic comparison harness is pinned to the same target identity.
Historical runners remain unchanged.

## Acceptance

- retained v1.1/v1.2 safety contracts stay PASS;
- v1.3 and active handoff tools match `validation_target_v010.json`;
- stale installed metadata is MISMATCH, never PASS;
- no native renderer source changes belong to this diagnostic block;
- mock/CI PASS is not a real After Effects PASS.

Real AE execution of v1.3 remains NOT RUN until host evidence is returned.
