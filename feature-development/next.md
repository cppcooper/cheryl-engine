# U9 bounded atomic terrain batches and next action

TILEMAP-U9 starts clean at completed U8
`79c02fbc7f903e7e87e5afc2dbf4b70fa1797032`, after inspecting its ready receipt,
public contract, shared ownership and saved 64/64 passing checks. It adds one
Engine batch operation, preserves all 64 prior names and passes one accumulated
73/73 selection, zero failed/skipped, 42.53s, exit 0. Header-first compilation and
six-operation import/export checks pass. This is CPU/shared-link evidence, not
coordinator source acceptance or graphics acceptance. U7's older evidence stays
61 unique passes across an initial 60/61 and correction/1-case rerun.

## Public caller and selected contract

```cpp
#include <cheryl/maps/terrain-map.h> // first include; link only Cheryl::Engine
const std::array stroke{
    CE::Tilemaps::TerrainEdit{{0, 0}, {1}},
    CE::Tilemaps::TerrainEdit{{1, 0}, {1}},
    CE::Tilemaps::TerrainEdit{{0, 0}, {2}},
};
auto affected = terrain.edit_batch(
    stroke, {.requests = 3, .affected_cells = 6, .work = 64});
// On the same serialized owner, resolve terrain.tiles() and add to the usual pass.
```

The [public header](../projects/engine/include/cheryl/maps/terrain-map.h) adds
TerrainEdit, TerrainBatchLimits and TerrainMap::edit_batch. The
[Engine source](../projects/engine/src/maps/terrain-map.cpp) owns implementation;
constructor/single-edit/batch share one derivation routine using the existing
Selection::select(BitmaskRule). No app terrain meanings, new error family,
World, storage or backend service appears. Existing edit() keeps exactly its
center/N/E/S/W order and preservation contract. New naming, last-wins duplicate
policy and budget units are delegated engineering choices with high behavioral
confidence, low user-taste confidence and public source/relink reversal cost.
Matching TM-006/007/011 in [decisions.md](decisions.md) and
[agent-notes.md](agent-notes.md) separate these choices from inferred preferences.

Validate every signed point before proposal construction; request-cap refusal
precedes scanning. Duplicate points are last-write-wins but no overwritten
request hides invalid coordinates. All uint32 TerrainLabels remain valid,
including zero empty and UINT32_MAX; there is no representable invalid label.
Coalesce owned final labels, prune centers equal to their original label, then
resolve the unique center/cardinal union against final semantics. Intermediate
masks need not exist if final masks resolve. No cached visual ID infers semantic
equality; equal visual output still invalidates when labels change.

Result ordering is stable: first request occurrence of each net changed center,
visit center/N/E/S/W, append each in-bounds cell on its first encounter. A later
center may already have appeared as a neighbor. No diagonals or out-of-bounds
points. Positive uint32-bounded coordinates plus +/-1 fit int64; extreme signed
inputs reject before neighbor arithmetic. X-right/Y-down, metadata bit order,
1..4 unique cardinal directions, empty outside fallback, original selection
targets and explicit copied target-to-logical mapping stay unchanged.

## Caps, ownership and publication

Every call supplies explicit maxima; zero caps are legal. requests counts every
record, including duplicates/no-ops/cancellations. affected_cells limits the
unique returned/recomputed union, including empty neighbors. Work units charge:

- one per admitted request, including no-op/duplicate/cancellation;
- one per net changed center and each of its four cardinal candidate visits,
  including outside/duplicate candidates;
- one per unique affected cell resolution, even empty (including selection if
  nonempty), plus one per actual selector sample, even outside.

For a successful call W=R+5*C+A+k*F, where R=input records, C=net changed centers,
A=affected union, F=nonempty final affected cells and k=authored cardinal bit count.
The implementation debits before each unit instead of computing that product or
sum, avoiding size_t overflow. Empty input costs nothing; all-no-op/cancelling
input needs only R units, no affected budget and no selection. Selection calls
are bounded by A, sampling by k*F<=4*A. Work units are logical visits, not allocator
bytes/comparison instructions/time; ordered lookups add logarithmic factors in
bounded request/affected sets. Scratch/output are O(R+A), never sized from cap
values or a full-map copy. SIZE_MAX caps with tiny real input stay tiny; no OOM
or fake huge-span probe is used. Allocation errors propagate before publication.

Complete owned label overlay, visual results and returned points precede any
assignment. A later coordinate, mask, mapping or cap refusal preserves every
semantic label and every visual layer. Commit uses prevalidated fixed private
storage, no remaining allocations or refusals. No caller span or sampler survives.
Input buffers may be changed/destroyed after return, and output owns its points.
Serialize reads/edits/preparation; retained old frames own copied packets/resources,
with observation outside writer/recycle scopes. This is atomicity for one terrain
batch, without undo, tick/frame rollback, concurrent publication or thread service.

## Focused evidence and exact execution

Six new tilemap_terrain cases check literal overlap/final-label results, stable
first occurrence including an initial no-op request, all uint32 labels, sparse
rules that reject sequential intermediates but accept final adjacent states,
exact/one-short request/affected/work limits, subset/outside sample charges,
empty/cancelling/no-op budgets, late mask/target/mapping/bounds refusals, and 320
batches versus independent full-grid semantic-mask and Manhattan-union oracles.
The reference calls neither incremental helpers nor the production selector.
Domains include 5x3, 1x4, 4x1, 1x1, reordered bits and shared visual IDs.

Two new [retained-frame cases](../projects/engine/tests/unit/src/tilemap-consumer.cpp)
prove a 9-to-8 packet edit with explicit authored order, input destruction, full
sibling/other-layer independence and retained old/new matrices/ranges/resources.
Seven initially valid refusal fixtures cover request/affected/work caps, a later
invalid point, missing mask, SIZE_MAX target and an unused unmapped target. No
partially painted stroke enters new frames. Weak resources expire only after
last frames/vectors release. Native bind/draw/upload counters remain zero.

The [normal Engine consumer](../projects/engine/tests/terrain-consumer/main.cpp)
retains values/errors/refusal names and adds batch. It includes the public header
first, links only Cheryl::Engine, exercises owned labels/output/late bounds errors
and imports the new operation. Its existing neutral header probe compiles first
without GL/GLFW. No consumer macro patch, private implementation copy, app/JSON
policy or alternate archive is used. The broad consumer family stays OFF; this
narrow shared-only check remains under CHERYL_BUILD_TESTS.

Preserve build/tilemap-u1-debug: GNU 16.2.1 Debug/shared/WARN/tests, policy minimum
3.5, previous optional/native/demo/aggregate/acceptance/consumer OFF selections,
sanitizer flags and all incremental outputs. Only tests-engine and the narrow
consumer/dependencies build, one job. All native work uses scripts/build with the
normal shared queue, adaptive monitor, no swap and 4 GiB admission floor. There
is no admission refusal, resource bypass, download, new dependency/pin or display
attempt. Only invocation-local detect_leaks=0 applies for the recorded LSan
tracing failure; ASan/UBSan/halt_on_error stay enabled without diagnostics.

```sh
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug \
  --target tests-engine tests-terrain-consumer --parallel 1
/mnt/projects/cheryl-features/scripts/build -- python3 build/tilemap-u1-debug/u9-check-ownership.py
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug -N \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.)'
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.)'
```

| Operation | Immediate exit | Result / complete local log |
| --- | --- | --- |
| Initial formatting | 0 | u9-format.log |
| Build | 0 | u9-build.log: Engine/runners link; neutral header compiles; no warnings |
| Added duplicate/cancellation cap assertion build | 0 | u9-build-cap-check.log; no warnings |
| Actual ownership check | 0 | u9-ownership.log: six Engine operations, diagnostic RTTI, sole source producer |
| Discovery | 0 | u9-registration.log: 73 unique names, exact 64-name retention |
| Accumulated tests | 0 | u9-ctest.log: 73 pass / 0 fail / 0 skip, 42.53s |
| Format checks before correction | 1 / 1 | u9-format-check-initial.log (overlong trailing comment), u9-format-check-shortened.log (adjacent comment alignment) |
| Comment-only format correction / final check | 0 / 0 | u9-format-correction.log / u9-format-check.log |

After execution only a trailing comment loses one word and adjacent comment
alignment changes. u9-comment-correction.log verifies normalized C++/assertion
tokens and line count unchanged; no behavior is changed or extra runtime pass
claimed. Formatter checks all changed C++ inputs. Full formatter/build/native
argv and immediate-exit summaries are in u9-commands.sh/u9-results.json. Saved
u9-LastTest.log preserves complete test output, registration correspondence JSON
compares exact names, and nm/readelf/link dumps retain actual ABI evidence.
All artifacts are ignored clone-local build/tilemap-u1-debug/u9-*.

Registration: 20 tilemap_consumer + 15 tilemap_submission + 14 tilemap_terrain +
20 tilemap = 69 unit names, plus four public consumer modes = 73. Engine exports
constructor/destructor/label/tiles/edit/edit_batch; unit executable and consumer
object import all six. Diagnostic RTTI/destructors remain Engine-owned; GNU PIE
loader COPY relocations for Engine-provided RTTI/vtables are the recorded U8 ABI
behavior, not an alternative source producer. readelf records libcheryl-engine.so.4;
normal Ninja linkage and compile requirements contain no test-owned terrain
source or private app paths. New method/aggregates require matching rebuilt
Engine/consumer versions; TerrainMap data fields/layout are unchanged.

## Limits and next bounded action

The unchanged U4 X11 :1 probe exited 255, `Error: unable to open display :1`.
It is not retried. Real input/display, app asset decode/upload, capture, graphical
teardown and leaks remain unverified. Installed SDK/export-manifest, Windows/
other compiler/STL, static and Release ABI acceptance are also unverified.
Source API comments are checked against implementation; no reference rendering
or public-guide campaign is run. No demo compiled input changes; no demo rebuild.

U9 has no unresolved CPU/build/link prerequisite. Commit its ready execution
receipt with the coherent implementation and records, then stop. Next is
coordinator review and finite-controller selection of pinned U10 private demo
terrain mode after this run drains. That later unit can consume this batch API
under existing editor ownership/caps; it is not started here. Preserve outputs,
leave branch transport to publisher, and perform no sibling integration,
resizing/chunking, undo, storage, backend or recurring-unit work.
