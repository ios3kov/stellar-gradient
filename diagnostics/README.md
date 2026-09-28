# Stellar AE diagnostics — host handoff, not a product release

2026-09-28. The user has After Effects on their Mac and supplied Cosmic.aex.
The reference is now available for static analysis; receipt and format checks are
in `reference_receipt.json`. SHA-256 matches the previously documented reference.
It is a Windows x86-64 PE DLL, not a macOS plugin. Do not upload the proprietary
binary, licensing code or extracted shaders to this repository.

## Current candidate vs diagnostic tool

The last code-side tested plugin remains commit
`deec78835801c5bf6f1aa44c772b508e43697b11`, v0.9.6. This diagnostic-only change
does not rebuild, install or certify that plugin. It tests whichever standalone
`StellarLabs.StellarGradient` is registered in the AE instance where the user runs
it. The candidate Build ID in the report is a TARGET, not an observation. Actual
loaded Build ID stays NOT_VERIFIED. An old installed version must never be
reported as evidence for the newer candidate.

## One local execution

Save any current work yourself, then open an empty, unsaved project in AE.
Use **File > Scripts > Run Script File** and select `Stellar_AE_Diagnostics.jsx`.
If AE file-write permission is disabled, explicitly enable **Preferences >
Scripting & Expressions > Allow Scripts To Write Files And Access Network**.
The script never changes this permission. No Terminal, Python, remote connection,
new plugin installation or credentials are required.

It writes one new `Desktop/Stellar-AE-<run ID>/` directory containing report.json,
append-only events.jsonl and up to four 128x96 PNG captures. Compress this directory
and return it in the development conversation. No automatic upload is performed.
No user project names, project paths, third-party license data or unrelated
installed effects are collected. AE/OS versions and Stellar/Cosmic effect
registration metadata are collected. Review the report before sharing it.

## Checks and meaning

The script checks standalone effect registration, apply/parameter initialization
(including capture of a 25::3 failure), repeated application/default-tree stability,
CPU requests at 8/16/32 project bpc and an Auto request at 32 bpc. Captures use the
documented Render Queue, an existing PNG template, disabled source XMP and no
post-render import. No templates are saved or edited. Each capture needs DONE,
exactly one fresh PNG, a PNG signature and 128x96 IHDR dimensions.

`SMOKE_ONLY_PASS` does NOT certify pixels, HDR, alpha, an actual GPU path, UI
preset callbacks, Undo/Redo, save/reopen, clean installation, restart, migration,
performance or complete Cosmic equivalence. PNG is not an HDR numerical fixture.
The script never infers a GPU dispatch from the engine menu's value.

Missing effects/permissions/templates are BLOCKED, not PASS. Original preset
render comparison remains NOT RUN. The Windows reference cannot supply a native
Mac render. Host testing is NOT RUN until actual returned evidence is examined.

## Safety and lifecycle

The script refuses a non-empty/saved project or existing/rendering queue before
any host mutation. It creates only its own composition/solid/queue items; closes
its synchronous undo group in finally and restores the original project bit
depth. Cleanup targets only owned objects and the owned empty solid folder, never
all project items. It never saves/closes a project, changes global preferences,
clears shared caches, installs/uninstalls plugins, kills processes or uses the
network. It may leave the initially empty project marked modified; no user
project file is written. Cleanup failures are FAIL and are not silently hidden.

Every run has a fresh directory and ID. Partial events retain the last operation
if AE crashes. A 120-second budget is checked BETWEEN host calls; JavaScript
cannot preempt an unresponsive native callback or guarantee crash cleanup. This
is a disclosed diagnostic limitation, not a release-gate waiver. Do not kill an
AE session with unsaved work. Escape/Stop can cancel an ordinary render.

## Validation

`node diagnostics/test_runner.cjs` executes the actual JSX in an isolated fake
AE/filesystem. 14/14 harness cases PASS locally: success, busy/saved project,
missing plugin, write denial, initialization failure, failed canAdd, repeated
default drift, missing PNG template, render exception/cancel, invalid PNG/size,
and unique runs with GPU/HDR/identity limitations retained. These are mock tests,
NOT a real Mac/AE run. No native render or plugin source changed.

## Research

Adobe documents direct JSX execution and user-granted file-write permission:
https://helpx.adobe.com/after-effects/desktop/automate-in-after-effects/automate-animation/scripts.html

Adobe-origin scripting API reference: app.effects exposes registration metadata,
not the runtime Build ID:
https://ae-scripting.docsforadobe.dev/general/application/

Documented render APIs; OutputModule must be reacquired after settings changes:
https://ae-scripting.docsforadobe.dev/renderqueue/outputmodule/
https://ae-scripting.docsforadobe.dev/renderqueue/renderqueueitem/
https://ae-scripting.docsforadobe.dev/renderqueue/renderqueue/

The undocumented saveFrameToPng shortcut is deliberately not used.
