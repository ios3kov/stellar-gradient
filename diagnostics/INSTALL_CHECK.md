# Mac installation investigation — 2026-09-28

## Observed baseline (not an inferred installation state)

The user's actual AE report is now available. SHA-256 of the supplied report.json:
`338e1ca48849e31107daebe8b036a526ec53a6712ad6ee26d4111e209b763e51`.
AE reports version 25.6x101 and Macintosh OS 26.6.2/64. Its effect registry includes
`Loophouse Cosmic` version `1.0x1`, but not `StellarLabs.StellarGradient`.
The diagnostic stopped with BLOCKED before rendering; captures is empty and
loaded_build_id remains NOT_VERIFIED. This does not prove that Stellar files
are absent, nor that registered Cosmic can render a reference successfully.
The previous statement that no Mac Cosmic registration was available is superseded.

User-approved scope: investigate Stellar installation/loading; leave Cosmic,
projects, preferences, security settings and existing plugin files unchanged.
Rules rechecked: FSTR-Line DEVELOPMENT_RULES.md, blob
`a1760fde8763f789b50b91c20407938b4fcaea4a` (unchanged).

## Checks fixed before implementation

1. Recheck the unchanged internal candidate, not a freshly rebuilt substitute.
2. Collect known install/staging paths, duplicate/disabled-folder hints,
   on-disk version/Build ID, executable format/hash, signature/quarantine
   observations and only Stellar mappings of the current user's AE processes.
3. No install, removal, native-plugin execution, download, preference/cache edit,
   privilege escalation, quarantine removal, re-signing or process termination.
4. Missing/inaccessible/truncated coverage remains explicit; no global absence,
   runtime-version, Gatekeeper-approval or release claim from a disk inventory.
5. Test the collector on isolated fixtures before handing off its exact source.
   Run Bash syntax and fixture tests on Linux and macOS; macOS also exercises
   real built-in commands on an owned invalid/unsigned bundle, without AE.

## Internal candidate validation completed here

The previously downloaded candidate archive SHA-256 was independently rechecked:
`552fae97e2d268c240f149755c6902b62e1a9414eb6d0a34ae5b8a7896ea46f9`.
Inner plugin ZIP SHA-256:
`2ad23b5b075b3c8c453568fc74c724c552a4d7b42c25cb548110a69ab7696a54`.
All six signed-payload file hashes match the manifest. Plist version is 0.9.6;
commit is deec78835801c5bf6f1aa44c772b508e43697b11 and its compiled Build ID matches.
LLVM inspection identifies ARM64 Mach-O, minimum macOS 11.0, SDK 15.5, and exports
EffectMain and PluginDataEntryFunction2. External load dependencies shown are
Apple system frameworks/libraries; the CI path shown as the dylib's own ID is
not itself an external missing dependency. No candidate bytes were changed.
These are static checks, not an AE load test, signature policy approval or proof
that the candidate is installed on the user's machine.

## Collector use

Save `Stellar_Mac_Check.command` in Downloads and run:

```sh
/bin/bash "$HOME/Downloads/Stellar_Mac_Check.command"
```

AE may remain open. The collector creates one private, uniquely named
`Desktop/Stellar-Mac-Check.XXXXXXXX/report.txt` and makes no other filesystem
writes. Return that report in the development conversation. Commands use macOS
built-ins: no Python, Xcode, sudo or new software is required by the collector.
System permission denial is recorded or reported without changing permissions.

Paths under HOME are redacted and report strings are shell-escaped. No unrelated
plugin metadata, project names/content, account credentials, system-wide logs,
quarantine-origin data or signer identities are saved. Review technical paths
in the report before sharing. No upload is automatic. Only bundles whose names
contain Stellar are inspected; renamed bundles can be missed. Symlinks are not
traversed. Common/system and historical user MediaCore locations, standard AE
folders and the running AE's folder are candidate search roots, not guarantees
that every such location is loaded by AE. A shallow Downloads inventory is
explicitly separate from installed-location counts. Search is bounded to 2,500
directories and 32 bundles; partial coverage is labelled. A shell export marker
is only a marker, not proof of the active implementation. Process architecture
and mapped files remain NOT_VERIFIED where tools cannot establish them.

There is no hard timeout for the read-only OS inspection commands. Ctrl+C stops
collection without killing AE; incomplete reports must not be treated as PASS.
The collector deliberately does not read/change the Effects Manager preferences.
Actual enabled state, loaded runtime Build ID, reference renders and full release
acceptance remain unverified until the relevant actual-host evidence exists.

## Evidence / identity

Collector SHA-256:
`48ed72555dc97e6eedfb483fe67526329460d1ce260b2093f8c6df565aecf7c3`.
Local Linux: Bash syntax PASS; 21 fixture checks PASS; one real-macOS-command
case is NOT RUN locally (requires macOS). Dedicated CI runs 21 existing cases
plus the report-write regression (22 total) on macOS.
CI result is recorded separately after execution; it is not assumed here.
Tests cover unchanged unrelated Cosmic files; invalid/symlink inputs; duplicates;
disabled folder hints; malformed/escaping plist values; invalid signature;
quarantine observation without deletion; shell marker; staging-only downloads;
redaction and process filtering; incomplete depth coverage; fresh report paths;
unsupported OS, missing Desktop and report-write failure. Completed collection has
an explicit footer; a write failure is not announced as success. No native/plugin
source is changed.

## Research used (not observations of the user's Mac)

Adobe SDK guide: standard install roots, recursive loading and disabled directory
naming conventions:
https://ae-plugins.docsforadobe.dev/intro/where-installers-should-put-plug-ins/

Apple code signing guide: verify/display operations are distinct from signing:
https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/Procedures/Procedures.html
