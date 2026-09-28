# v0.9.9 — Metal buffer-layout validation

2026-09-28. Internal development checkpoint, not release approval.
Rules reviewed: FSTR-Line DEVELOPMENT_RULES.md, blob
`a1760fde8763f789b50b91c20407938b4fcaea4a`, especially 8–10 and native section 23.
Upstream work branch: `fix/stellar-release-gate`; main/Hot Loader are untouched.
Last native baseline: `d7e035ddf8751deb1c43c54d1cff050036890d64`, v0.9.8.
Current upstream documentation parent: `79e17721e612bd3c3d7aae457019256b4444c7c6`.

## Defect and scope

The baseline checks `rowbytes % sizeof(float) * 4 != 0`. This is remainder by
four followed by multiplication, not remainder by sixteen. For a two-pixel row,
36 bytes passes and is truncated to a two-pixel (32-byte) pitch. Subsequent rows
are then addressed differently from the supplied layout. No actual user crash
or GPU memory violation is asserted from this static finding.

The same entry did not verify row capacity or MTLBuffer length and formed signed
rectangle differences without range checks. This change rejects unsupported or
unsafe descriptors before pipeline access, texture allocation or command-buffer
creation. It does not repair Turbulence, Softness, Depth shape or visual parity.
CPU rendering/shader math, parameters, ABI and all goldens are unchanged. Package
version becomes 0.9.9; only the root Cargo lock version changes, no dependencies.

## Acceptance chosen for this change

- Positive dimensions and rowbytes; a row must fit all width BGRA128 pixels and
  have a complete float4 pitch. Validate input and output separately.
- Check the final logical pixel against each MTLBuffer.length. Permit padding and
  different pitches. Do NOT require padding after the final logical pixel.
- Widen arithmetic before multiplication/subtraction; reject uint32 MSL index
  wrap and signed geometry overflows. Output crop stays inside the work texture.
- Preserve input regions extending beyond work (intentional zero fill) and empty
  semantic bounds. Do not silently clamp descriptors, copy buffers or change data.
- Invalid descriptors return the existing -1 error without enqueueing GPU work.
- Keep all prior tests and goldens; add a production-entry reproducer that fails
  against the pinned old wrapper, then passes against the corrected wrapper.

The existing float4 representation cannot express a four-byte-but-not-sixteen-
byte row pitch. Such layouts now fail explicitly rather than being misaddressed.
This is NOT a general AE requirement for 16-byte CPU pointer alignment. Support
for arbitrary byte pitches would require a separately designed shader change.

## Implementation

`MetalValidation.hpp` is pure C++17; computes pitches and required byte spans
without allocations, signed overflow or Metal dependencies. All checks are
constant work per request, not per pixel. `MetalBridge.mm` queries the two real
MTLBuffer lengths only after scalar checks, then uses the validated dimensions
and pitches. It retains the existing return-code mapping and asynchronous GPU
submission. No synchronization/wait, render retry or CPU fallback was added.

Geometry checks cover the existing signed pack_gpu expressions and load_bgra
coordinate subtractions; shader uint32 pixel offsets must fit. No assertion is
made about arbitrary invalid Objective-C pointers: the host must still supply
valid objects of the documented types. GPU driver errors, device compatibility,
CPU bridge input validation and aliased-buffer behavior are outside this patch.

## Tests and evidence boundaries

`tests/metal_validation.cpp`: exhaustive small planes against an independent
byte-span oracle, index limits without large allocation, unequal padded rows,
exact/short capacities, crops, translated/empty/overflowing rectangles and
100,000 wide scalar samples under sanitizers. It runs in the standard core suite.

`metal_validation_host_test.mm`: calls the actual sg_metal_render entry on macOS
with Objective-C length/queue test doubles. Invalid cases must return -1 and
never query the queue; valid cases reach a deliberately nil command buffer and
return -3. This verifies the entry guards, NOT real GPU execution or AE loading.
The 36-byte-row reproducer uses the exact pinned old wrapper and must fail with
result=-3, commands=1; corrected wrapper must return -1, commands=0. CI enforces
both results, runs new entry tests with ASan/UBSan, and retains all logs.

Local Linux strict, ASan/UBSan and TSan core suites: 10/10 PASS each. Identity
fixtures: 10/10 PASS. Source freeze: 59 files PASS. Previous base/grain goldens
are unchanged. Actual macOS entry/compile/package CI is pending at commit creation
and is reported separately after observation. Actual new AE/GPU execution remains
NOT RUN. The user's v0.9.6 runs must not be relabelled as v0.9.9 evidence. No new
user diagnostic/install is requested, no user files/preferences were modified.

## Research

Adobe-origin SDK guide: rowbytes is a byte stride, input/output may differ, and
CPU pointer alignment is not guaranteed. We preserve padding where representable:
https://ae-plugins.docsforadobe.dev/effect-basics/PF_EffectWorld/

Apple: MTLBuffer.length is the logical byte size, not a pixel count:
https://developer.apple.com/documentation/metal/mtlbuffer/length

No additional dependency or copied third-party implementation is introduced.
