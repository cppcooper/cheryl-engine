# U3 checkpoint and next unit

TILEMAP-U3 starts at coordinator-accepted U2 checkpoint
`c77496f0adace3e76c9a59fc573a1a0ef138eb63`, with a clean index/tree. This unit
adds only a synthetic pass consumer and clarifies the existing bind-once source
contract. Six consumer cases compose two independent 3×2/two-layer maps through
existing FrameWriter/PassWriter APIs. The final Debug ASan/UBSan selection passed
36/36 with no skips, including all 30 U1/U2 regressions. There is no unresolved U3 assertion/build failure.
No graphical consumer, production behavior/API, dependency pin or build-file
change belongs to U3. The next unit is proposed below, not activated.

## Owners and decisions

- [Consumer cases](../projects/engine/tests/unit/src/tilemap-consumer.cpp):
  `tilemap_consumer.{caller,sequence,snapshots,resolver_failure,group_failure,retention}`.
  The small caller is also in [agent-notes.md](agent-notes.md#small-u3-pass-caller).
- [Submission header](../projects/engine/include/cheryl/assets/submission/tilemap2d.h):
  comments only; retained handles do not deep-copy/freeze an asset. Existing
  declarations/layout, CPU Map, binding/packet implementation, and production
  frame/pass/backend owners remain unchanged.
- Matching [reader decisions](decisions.md) and [detailed decisions](agent-notes.md)
  update TM-002/005/006/008, retaining all stable TM-001–010 IDs.

TM-002 is an explicit caller obligation: bound source metadata must remain
unchanged through **every alias** for the lifetime of **any binding** retaining
it, including copied tables. A const shared handle does not freeze an Asset.
Per-call table/asset/resource stability obligations still apply. Synchronizing a
metadata mutation between resolutions is insufficient; validation occurs at add,
and no hot replacement, metadata-copy layer or ongoing revalidation is introduced.
Fixtures construct actually const assets and obey this contract. Passing tests do
not mechanically prove arbitrary external aliases comply with it.

The consumer deliberately uses direct calls, not a scene abstraction or new runner:
begin a pass, copy its semantics/parameters/constraints into Context2D, resolve one
layer at a time, then add each vector in caller order. Writers end before frame
observations. Borrowed frame spans/packet references never survive a recycle;
explicit retained snapshots are owned vector copies. No concurrent writing,
reading/recycling or graphics-domain retirement is exercised.

## Observable consumer checks

- `caller`: west ground/detail then east ground/detail add 3+2+2+1 packets.
  PassWriter assigns 0–7 consecutively across vectors, preserving each vector's
  row/column order. Projection, view, custom pass weight and constraints are
  copied; changing the original caller inputs after preparation cannot change
  frame/packet values. Every model retains Z=0.
- `sequence`: reversing the four caller groups changes the expected order while
  keeping within-group traversal. A distinct retained image/material pair moves
  from packet position 0 to position 5, so texture/material grouping could not
  silently pass. Expected ranges/positions are authored constants, not derived
  by iterating the resolver output.
- `snapshots`: prepare an older frame, end writer scope, paint one west-map cell
  and move only the west view, then prepare a separate newer frame. Only the newer
  west ranges/matrices change. The older packet payloads/resources/order, every
  east cell and all three east packet payloads/order remain unchanged.
- `resolver_failure`: insert two valid prior packets and successfully resolve the
  west layer before changing only a later visible cell to missing ID 999. The
  negative assertion calls only the resolver; a geometry metadata counter proves
  its earlier valid cell reached packet validation. No failed result is inserted,
  and all pre-existing pass payloads/order survive.
- `group_failure`: resolve a valid two-packet group, validate both packets against
  the actual pass constraints, then invalidate only the later packet's
  vertex_count (4 to 0). Existing group add rejects it, appending neither packet
  and preserving both prior draws/order. This is distinct from resolver rejection.
- `retention`: destroy maps, bindings, provider and all fixture resource owners
  while the frame alone remains. Asset metadata expires, but geometry/image/
  material/pipeline weak handles remain live. Copy the frame's packets to an
  owned vector, then recycle the frame; the vector still retains resources.
  Clearing that final vector expires all four resource weak handles. Checks run
  outside writer/borrowed-view lifetimes.

Synthetic bind/draw counters and provider image/font/geometry/program upload-entry
counters stay zero. Stand-ins are constructed directly; the consumer receives no
provider capability. This bounds the evidence to local synthetic resources, not
a native backend, visual result or global upload interception. Resource release
here establishes shared ownership release, not a LeakSanitizer pass or real GPU
teardown. Allocation exhaustion/OOM is never used as a probe.

## Actual commands and results

Reused `build/tilemap-u1-debug` and its incremental outputs. Cache remains Debug,
GNU 16.2.1, shared Engine/tests/WARN ON, CMake policy minimum 3.5, and all assigned
optional native/module/app/docs/test families plus CHERYL_STARTUP_OPENGL OFF.
Existing unit globs register the new source in tests-engine; GoogleTest discovery
adds the tests-engine prefix. The first build's glob-triggered CMake regeneration
is expected; no new configuration/directory or runner was created. The new unit
compile database entry retains `-g -Wall -fsanitize=address,undefined`.

Every configure/build/native check uses the existing wrapper and one job. It
keeps the fixed 4 GiB aggregate, 3 GiB throttling, 2 GiB worker/no-swap limits and
4 GiB MemAvailable admission floor. Live sessions are retained through ordinary
lock queueing. No exit-75 refusal occurred; an actual refusal must be recorded
exactly and checkpointed idle without an unchanged retry.

```sh
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug --target tests-engine --parallel 1
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug -N \
  -R '^tests-engine\.(tilemap|tilemap_submission|tilemap_consumer)\.'
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error -R '^tests-engine\.(tilemap|tilemap_submission|tilemap_consumer)\.'
```

Initial build and discovery exited 0. Registration selected exactly 36 cases:
6 tilemap_consumer (#200–205), all 15 tilemap_submission (#206–220) and all 15
tilemap (#221–235). The initial native run passed 36/36 in 28.84 seconds. Review
then strengthened the frame-only lifetime observation before adding a vector
owner, and a rebuild/rerun passed 36/36 in 31.14 seconds. The same commands are
used for the final distinct-image/material sequence fixture. Its build and test
commands both exited 0; all 36 cases passed in 28.26 seconds, with no skips or
address/undefined sanitizer diagnostics in the full test log. No assertion or
sanitizer failure occurred in any of these completed U3 runs.

Complete diagnostics are preserved in this clone's build directory:

- `u3-build.log`, `u3-registration.log`, `u3-ctest.log` for the initial run.
- `u3-build-final.log`, `u3-ctest-final.log` for frame-only lifetime checks.
- `u3-build-mixed.log`, `u3-ctest-mixed.log` and final
  `Testing/Temporary/LastTest.log` for the final mixed-resource fixture.

The builds emit no new source warnings. CMake's existing optional
`Could NOT find libdwarf` diagnostic does not prevent build/test completion.
clang-format dry-run/Werror and Git whitespace checks pass. No files are fetched,
installed, replaced from protected caches or run in protected/sibling workspaces.
Earlier U1/U2 exact configurations/results remain in committed next.md history at
`dec1ec55` and `c77496f0`; all `u1-*` and `u2-*` local logs are preserved.

ASan/UBSan stay enabled with halt_on_error. Only command-local leak detection is
disabled for the already diagnosed LeakSanitizer tracing limitation:

```text
LeakSanitizer has encountered a fatal error.
HINT: LeakSanitizer does not work under ptrace (strace, gdb, etc)
```

Leak checking remains unverified. There is no global sanitizer/configuration change.
No broad suite, device/GUI run or documentation generation is performed.

## Confidence, limits and next action

Engineering confidence in the scoped composition/failure/lifetime contracts is
high from source plus focused synthetic results. Alias stability is the user's
explicit bind-once contract, not inferred taste or runtime enforcement. Fixture
names/data and exact consumer shape are routine reversible test choices, with low
personal-preference confidence. Reversal cost is local to these cases and records;
no production layout or public callable changes were introduced.

Linux GNU shared Engine use is exercised. Leak checking, installed SDK/Windows
exports and other compiler/STL/GLM ABIs, Release, real native-domain retirement,
concurrent publication, shader execution, performance and visual edit feedback
remain unverified. A successful local checkpoint is not graphical acceptance.

The next proposed bounded action is **U4's minimal display fixture**, requiring a
later coordinator assignment: use an existing-demo helper to display two finite
in-memory maps with two layers through the already exercised pass sequence.
Validate suitable existing local tile artwork/targets before using them, then
observe actual layered order and pivot/placement visually. Editor/picking
controls, persistence, auto-tiling, resize and chunking remain later work. No art
download, module enablement, runtime startup or next-unit work starts in U3.

Commit only this consumer, header comment and internal records. Leave upstream
transport to the coordinator publisher for assigned feat/tilemap; do not claim a
remote push was performed here. Stop after the coherent U3 local checkpoint.
