# Source freeze v0.6

Date: 2026-09-26
Plan point 4/10: COMPLETE (source freeze; host build still pending).

The codebase is frozen at v0.6.0 build 6 after the final source audit, fixes and regression pass.

## Freeze rule

No source changes from this point unless one of these gates produces evidence that requires a change:

1. native macOS compilation against the installed Adobe After Effects SDK;
2. plug-in load/runtime behavior inside After Effects;
3. Metal Instruments/profile data;
4. CPU↔Metal visual parity test on Apple Silicon;
5. controlled Cosmic comparison.

Any post-freeze change must document the host failure/profiler evidence, add or update a regression test where possible, and regenerate `SOURCE_FREEZE_SHA256.txt`.

## Frozen quality/performance contracts

- CPU FP32 golden reference is authoritative.
- Auto/Final use FP32 intermediates; Preview alone may use FP16.
- C++ Release fast-math is disabled.
- Metal runtime fast-math is disabled.
- No image-quality reduction is allowed as a performance fix.
- Persistent AE parameter disk IDs 1..49 are immutable.

Exact SHA-256 hashes are recorded in `docs/SOURCE_FREEZE_SHA256.txt`.
