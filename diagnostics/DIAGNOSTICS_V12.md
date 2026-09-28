# Diagnostic 1.2 — explicit preflight and consent-based preparation

2026-09-28. Baseline branch commit 415fc7f4f42c2fd5aa4117f5f1d4cf41235e2b22.
Rules reviewed: FSTR-Line DEVELOPMENT_RULES.md, blob a1760fde8763f789b50b91c20407938b4fcaea4a.
Native candidate remains deec78835801c5bf6f1aa44c772b508e43697b11 (0.9.6).
This change does not install, rebuild or modify the native plugin or Cosmic.

## Evidence received, not a diagnosis of the user's project

Run Stellar-AE-1790618229120-623443347, runner 1.1. Report SHA-256:
250c8fbd397353000b6acd8abd546ca3fcf6fda2155862c0d1b03ffa77759f2d.
Events SHA-256: b364c7bf9ffd442f1de7761056eb4045fdbf8c4a61611c20d1ddcd70d49890ab.
Both files identify the same run. Events contain only start and the generic project
check BLOCKED at 8 ms. Every case is NOT RUN and captures is empty. No native
error is shown. The report has no per-condition state or environment section.
The source orders the initial guard before environment capture and before assigning
the expected project reference: this is evidence of the initial preflight failure,
not evidence of a later project-switch failure. It cannot distinguish a saved
project, existing items, a nonempty queue or an active render. Do not blame the user
or claim the renderer failed from this record. The historical result stays BLOCKED.

## Acceptance and approach

Preserve all eight cases, deferred scheduling, strict cleanup and existing gates.
Before any fixture mutation, record primitive per-condition state and readable
reason codes. Keep names/paths/content out of the project-state log. Fail closed
on unreadable state, active/paused rendering and disabled rendering. Never delete
existing items to manufacture a passing preflight.

If a readable idle project is unsuitable, ask once whether to create a new test
project. No is the default. Only after Yes, recheck project identity and rendering,
then call documented app.newProject(). Its native save dialog stays enabled; a
cancelled save/new-project action stops the run. No project.close(), automatic
save path, dialog suppression or DO_NOT_SAVE_CHANGES call is used. If AE presents
a save prompt, choose Save to retain work or Cancel to stop testing.
Revalidate the returned project; a nonempty startup template is blocked, not erased
or retried. There is no automatic preparation after the first scheduled test begins.
A project switch or active render between callbacks still stops the diagnostic.
No user cache, global preference, permissions, plugin files or security setting is
changed. The existing empty project can be used without a preparation dialog.

Reason codes and current state appear in report.json/events.jsonl, and the final
alert includes the blocking/error detail. Environment and registry capture precede
project preparation. The run reserves the old shared diagnostic key across dialogs,
preventing concurrent 1.1/1.2 tests. Save-dialog time is excluded from the 180-second
between-call execution budget (elapsed report time is still total wall-clock time).
Native callbacks cannot be preempted; this limitation has not changed.

## Validation and identity

Local Node: original 21 sequence tests on 1.1 PASS; same 21 on 1.2 plus 15 targeted
preparation tests PASS (36 on the new diagnostic). Fixtures cover individual
conditions, consent rejection, native save cancellation/exception, new empty
project success, unsafe/unknown state, missing permissions/effect, project/render
changes during consent, nonempty startup templates and between-task project changes.
These tests model host contracts, NOT actual AE or its Save dialog. Real 1.2 host
run: NOT RUN. The cause of the original guard result and the earlier 16-bit queue
return are NOT established. This is a diagnostic usability/correctness fix, not a
native-render repair. Loaded Build ID, GPU execution, HDR and release remain unverified.

New single-file diagnostic SHA-256:
290ec5f5f216b0c08e7e59fbeb312fec00be8f3ce21492a5a388c60589fb2f53.
CI results are recorded in the PR after execution, without rewriting this evidence
as though it referred to the native candidate. Older runners/reports remain intact.

## Use

Run Stellar_AE_Diagnostics_v1_2.jsx through File > Scripts > Run Script File.
There is no need to install the plugin again. The script can offer a fresh project
with consent rather than requiring repeated manual preparation. Wait for the final
window without editing the test project, then return the fresh Stellar-AE-* folder.

## Sources

Adobe-origin API reference, app.newProject and cancellation contract:
https://ae-scripting.docsforadobe.dev/general/application/#appnewproject
Project file/numItems types:
https://ae-scripting.docsforadobe.dev/general/project/
Render Queue rendering includes paused state:
https://ae-scripting.docsforadobe.dev/renderqueue/renderqueue/
ExtendScript confirm No-as-default:
https://extendscript.docsforadobe.dev/extendscript-tools-features/user-notification-dialogs/
