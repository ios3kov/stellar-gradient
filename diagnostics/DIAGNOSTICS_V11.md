# AE render-queue investigation — diagnostic 1.1

Date: 2026-09-28. Governing rules rechecked against FSTR-Line
DEVELOPMENT_RULES.md blob a1760fde8763f789b50b91c20407938b4fcaea4a.
Native candidate remains deec78835801c5bf6f1aa44c772b508e43697b11.
No native source, plugin payload, install, main branch or Hot Loader change.

## Actual user evidence (v1.0 runner)

Run: Stellar-AE-1790616660224-484225077.
Archive SHA-256: 6718d5f8283b3026537c2f18a1e6cb788df992554630a743f9812008daffd623.
Report SHA-256: 90ec705e80702f3bc0f7c5c83fa2e887326497ef75861b0006bb13f68df9f490.
Events SHA-256: 472a2af44308a4fd3a5e1d56a617ab6e8bb216b600bbd30dae3dd7a106ceea94.
PNG SHA-256: 49a72878dd92b8548686e1a356801c6ba23e6775e6dad02bb81dd749eef7c240.
Raw uploaded report/images remain in the conversation; no proprietary files are published.

- AE 25.6x101, macOS reported as Macintosh OS 26.6.2/64.
- Stellar registration 0.9.6x1 and Cosmic registration 1.0x1 observed.
- Effect initialization and repeated default-parameter tree: PASS on installed version.
- CPU-requested 8-bpc Render Queue: DONE and one 128x96 PNG. File decodes;
  all alpha bytes are 255. The image is strongly granular. No Cosmic frame or
  controlled expected image was supplied, so visual correctness is NOT VERIFIED.
- CPU-requested 16-bpc case: original report says FAIL, detail
  `Render did not finish: 3015`; no PNG exists for that case.
- CPU 32 and Auto 32 were NOT RUN because runner 1.0 breaks on the first exception.
- Fixture cleanup: PASS according to the supplied report.
- Loaded Build ID remains NOT_VERIFIED; the target ID is not a runtime observation.

## Interpretation, separate from observations

3015 corresponds to RQItemStatus.QUEUED, not ERR_STOPPED (3018).
Therefore this run does not demonstrate a 16-bit renderer defect. It demonstrates
an incomplete queue operation and insufficient runner diagnostics. The historical
FAIL record is preserved; it is not rewritten as PASS. No definitive cause of the
QUEUED return is established (host scheduling, settings, cancellation, etc.).

## Acceptance fixed for the diagnostic change

Use symbolic host status values; record the actual runtime enum mapping, status
transitions, onError messages where no existing callback is installed, and a
whitelist of render/output settings. Never present QUEUED as a completed frame or
as proof of a native plugin exception. Leave unexecuted cases explicitly NOT RUN.

One invocation runs eight independent cases: no-effect control at 8/16/32 project
bpc, Stellar CPU-requested at 8/16/32 bpc, Auto-requested 32, and a separate clean
8-bpc gradient with Bulge/Turbulence/Glow/Grain/Diffusion disabled by persistent
parameter IDs. The clean fixture does not replace the unchanged default fixture.

Each case uses a fresh composition/solid and its own deferred app.scheduleTask.
Undo is closed, fixture removed, bit depth restored and temporary callbacks removed
before scheduling the next callback. Recheck the same empty unsaved project before
all callbacks. No render retries; independent remaining cases can run after QUEUED.
A native error, user cancellation, invalid image or unsafe cleanup stops further tests.

This addresses a diagnostic gap. Deferred scheduling is not yet a proven fix for
the user's AE behavior. Native candidate remains unchanged and must not be reinstalled
for this diagnostic. Do not infer actual GPU/CPU execution from the requested menu.

## Safety and tests

No cache clearing, preferences changes, template edits, plugin install/replacement,
network access or process termination. Existing app.onError is left untouched;
when absent, a temporary handler records bounded sanitized error text and is restored.
Active rendering or a changed project is not forcibly stopped or deleted. After an
unexpected active-render return, cleanup is marked failed and the fixture is left.

Budget: 180 seconds between calls; cannot preempt a hung native callback. Only
non-repeating owned tasks are used. Double invocation is refused. Report checkpoints
and events preserve partial progress; file-write failure never reports completion.
No unrelated output paths or project contents are written to the report.

Before transfer: execute exact JSX in a fake-AE harness covering deferred boundaries,
3015, errors/cancel, prior hooks, invalid output, permissions, project changes,
cleanup failure, duplicate runs and honest identity/pixel limitations.
Local result: 21 harness tests PASS (some tests include several negative fixtures).
Real After Effects execution of version 1.1: NOT RUN. CI result recorded in PR #1
when observed, not assumed by this document. These are not real rendering tests.

## Use

Run Stellar_AE_Diagnostics_v1_1.jsx through File > Scripts > Run Script File in an
empty unsaved project. Let the automatic sequence finish without editing the project.
Compress the new Desktop/Stellar-AE-* folder and return it to this conversation.
Do not replace the installed native plugin. Old diagnostic 1.0 is retained only to
reproduce its historical reports; use 1.1 for new queue evidence.

## Research

Adobe-origin API documentation:
https://ae-scripting.docsforadobe.dev/renderqueue/renderqueue/ (render is synchronous)
https://ae-scripting.docsforadobe.dev/renderqueue/renderqueueitem/ (symbolic statuses)
https://ae-scripting.docsforadobe.dev/general/application/ (scheduleTask/onError/Undo)

Primary enum observations (AE 17.7; new runner records the current host constants):
https://gist.github.com/zlovatt/58c7a50805d509bf9e07b021234ae70a
https://github.com/orenazad/Render-Tracker (author's description of returned statuses)

No third-party implementation is copied. No new runtime dependency.
