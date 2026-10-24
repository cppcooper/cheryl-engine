# U10 private semantic editor checkpoint and finite-batch handoff

TILEMAP-U10 starts clean at completed U9
`bdead055f0d4a9667093e10df8abfc500bb1f7ff`, after inspecting its ready receipt,
Engine batch contract and saved 73/73 checks. It adds optional semantic editing
to the same private helper used by demo and synthetic consumers. Demo and CPU
runner build/link without warnings; one accumulated invocation passes 79/79,
zero failed/skipped, 45.48s. All 73 prior names, public header/export checks and
same-private-source proof pass. There are no post-test source corrections.
This receipt is execution evidence, not automatic coordinator source review or
graphical acceptance. U7's older two-run result remains separately historical.

## Small private caller and controls

```cpp
#include "tilemap-editor.h" // first in the existing synthetic consumer
DemoTilemapEditor editor;
// Existing synthetic record factories; the real demo forwards tick.input.records().
const std::array events{key(Key::B), key(Key::Digit3),
                        click(Mouse::Left, {40, 128}), click(Mouse::Left, {72, 128})};
editor.consume(events, {{400, 200}, {800, 600}});
editor.write(frame, bindings, image_style);
```

The [header](../../../projects/apps/demo/src/tilemap-editor.h) and
[implementation](../../../projects/apps/demo/src/tilemap-editor.cpp) remain private.
No duplicate event/transform algorithm is added to tests. Real demo forwarding,
required atlas loading, error reporting and pass preparation stay in
[tilemap-demo](../../../projects/apps/demo/src/tilemap-demo.cpp), reached by the existing
--tilemap-display mode and platform/simulation lifecycle. The initial mode remains
literal, retaining every original map/brush/control behavior.

| Control | Literal mode | Semantic terrain mode |
| --- | --- | --- |
| V / B | Select literal / terrain without converting cells | Same, preserving independent content/brushes |
| 1 / 2, G / T | West/east map, ground/detail layer | Same shared selection |
| 3–6 | Logical IDs 7–10, existing diagnostic colors | Opaque labels 1–4, not sprite IDs |
| Left / right Press | Paint ID / erase immediately | Append label / zero to the current bounded run |
| Middle Press | Sample literal ID, including zero | Flush then sample semantic label, including zero |
| WASD / wheel / R | Pan 32 units / anchor zoom to event pointer / reset view | Same shared view, no content reset |

These controls, names and grouping are delegated engineering choices with high
contract confidence, low user-taste confidence and private/local reversal cost.
TM-006/009/012 match [decisions.md](decisions.md) and [agent-notes.md](agent-notes.md).

## Ownership, palette and mode boundaries

Two literal Maps keep their original finite 3x2/two-layer seed. Four independent
Engine TerrainMaps (two instances times two layers) start empty. Each owns a
3x2/two-layer base, derives only its selected layer and leaves its complementary
layer empty. Rendering resolves the selected owner for each layer, without an
extra merged-map cache. map(index) always observes literal storage; terrain(index,
layer) borrows that semantic owner. Both modes retain separate brushes while
sharing view/selection. No literal ID is ever relabelled; switching modes never
reinterprets unrelated cells. The private helper is noncopyable/nonmovable.

The diagnostic palette explicitly declares N/E/S/W weights 1/2/4/8, mask%4 ->
selection targets 0..3 -> copied logical table {7,8,9,10}. Existing retained
bindings map those IDs to original atlas targets 0..3. Nonzero ID 7 draws target
zero; semantic/literal zero is empty. Four colors intentionally alias masks and
labels, without game/mountain meanings or new artwork. Palette inputs are copied
by Engine owners; changing/destroying caller config cannot affect them. The
private table accepts only IDs 7–10 or unmapped zero; invalid nonzero IDs reject
construction. Missing cases/targets/holes reject when needed, while empty owners
with empty cases/tables safely prepare zero packets. Engine contracts are unchanged.

Each admitted gameplay Press uses its own captured pointer; Release/Repeat,
unsupported/unadmitted records, missing pointers, final Pointer/Axis samples and
misses do not edit. No held-pointer history is invented. Collect semantic left/
right requests in order and flush before a keyboard Press, middle sample,
finite-delta wheel with valid pointer, and call end. Keyboard selection/mode/brush/
view changes act after successful flush. A wheel can flush before rejecting a
derived invalid zoom. Ignored records are not barriers. Middle sampling sees
completed prior edits, never a rendered ID.

Default run caps are 32 requests, six affected cells and 256 logical work units.
The helper checks count before growing pending input; Engine edit_batch then
validates/coalesces final labels and atomically publishes complete label/visual
results. Left/right cancellation in one run can succeed with empty rule/table,
where sequential edits would refuse the intermediate state. A failed run leaves
all its values unchanged. Earlier completed runs remain; there is no whole-tick/
frame rollback. Helper bad_request propagates. DemoTilemaps::update logs the
rejection to stderr and drops the rest of that tick; later ticks can continue.
That app catch is compiled/source-inspected, not executed by graphical input.
Allocation/other failures keep existing runtime handling; no error service is added.

Pointer conversion reuses independent logical-to-framebuffer scales, top-left to
Y-up flip, full viewport, inverse finite pan/positive zoom, map origin and Engine
computed-edge pick_cell. Sizes are captured together per tick, not historical per
event. Invalid/minimized sizes suspend all records/preparation, preserving data,
selection, mode and view until valid sizes resume. Invalid view candidates preserve
the old view. Platform constructs/loads/captures before the existing early return;
simulation serializes updates/preparation. Platform cleanup follows simulation/
frame completion. No records/spans/samplers/window/provider/frame views are retained.
Prepared frames own copied matrices/ranges/resources after owners/modes disappear.
Binding metadata stability through every alias remains the coordinator-selected
engineering contract, not an explicit user preference.

## Focused executed evidence

Six new cases in the [existing consumer](../../../projects/engine/tests/unit/src/tilemap-consumer.cpp):

- editor_terrain_events: two different pointers in one tick, Release/Repeat/final
  axis filtering, changed cardinal neighbor range, erase/sample/empty brush,
  labels sharing visual IDs, full 24-label/24-tile oracle and literal isolation.
- editor_terrain_modes: interleaved selection/layers/brushes and B/V transitions;
  separate stored content, complete semantic/prepared output, authored ground/
  detail order/alpha and retained literal frame.
- editor_terrain_view: unequal 2x/3x scales, exact internal/outer computed edges,
  nextafter neighbors, pan/zoom/reset/camera culling, invalid view/sizes suspension,
  shared mode/view and unchanged old frame. No live platform state is queried.
- editor_terrain_failures: initially valid full board, then two erases; earlier
  candidates resolve before missing mask/SIZE_MAX target/unmapped target refuses.
  Request/affected/work caps preserve entire current run; sibling/layer/literal
  snapshots and prepared old/new frames remain exact.
- editor_terrain_palette: copied configs, distinct labels sharing one visual ID,
  connectivity failure rather than visual inference, zero/cancelling empty owners,
  missing cases/tables and invalid private logical IDs.
- editor_terrain_frames: literal -> semantic paint -> erase -> literal, exact
  ranges/placements/order and complete semantic/tile oracle, old/new resource
  ownership through destruction and weak release only after last frame/vector.

The independent full-board reference uses literal neighbor comparisons/bit
weights; it calls neither Engine selection/batch nor cached visual connectivity.
Synthetic native bind/draw/upload counters stay zero. This proves CPU data/input
and prepared-packet contracts, without observing actual rendered pixels.
All previous literal-editor, terrain, map, packet/pass and public caller cases
remain selected and pass.

## Actual commands and results

Preserve build/tilemap-u1-debug: GNU 16.2.1 Debug/shared/WARN/tests/policy-3.5,
previous native/OpenGL/startup/demo/aggregate/acceptance/consumer OFF selections
and sanitizer flags unchanged. Preserve build/tilemap-u4-display-debug: its
existing Debug/shared/WARN, NativeGLFW/OpenGL/Startup/demo ON, tests/optional UI/
audio/workers/docs/other apps OFF, X11 ON/Wayland OFF. No fresh directory, cache
borrow, new module configuration, dependency/pin, download or install occurs.
Only demo and tests-engine/tests-terrain-consumer with needed dependencies build.
All native operations use scripts/build, one job, normal shared queue, adaptive
monitor/no swap/4 GiB admission floor. No resource refusal or limit bypass occurs.

```sh
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u4-display-debug \
  --target demo --parallel 1
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug \
  --target tests-engine tests-terrain-consumer --parallel 1
/mnt/projects/cheryl-features/scripts/build -- python3 build/tilemap-u1-debug/u10-check-ownership.py
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

| Operation | Immediate exit | Actual result / full local log |
| --- | --- | --- |
| App/case formatting | 0 / 0 | u10-format-app.log / u10-format-cases.log |
| Demo build | 0 | build/tilemap-u4-display-debug/u10-demo-build.log: compiled/linked, no warnings |
| CPU build | 0 | u10-cpu-build.log: runners/private source compiled/linked, no warnings |
| Final format check | 0 | u10-format-check.log: all five changed code files |
| Header/export/shared-source proof | 0 | u10-ownership.log, complete nm/readelf dumps |
| Discovery | 0 | u10-registration.log: 79 unique names, exact 73-name retention |
| Accumulated tests | 0 | u10-ctest.log: 79 pass / 0 fail / 0 skip, 45.48s |

Registration corresponds to 26 tilemap_consumer + 15 tilemap_submission + 14
tilemap_terrain + 20 tilemap = 75 unit names, plus four public caller modes = 79.
Exact name sets match prior/discovered/executed output. ASan/UBSan/halt_on_error
remain enabled without diagnostics; only invocation-local detect_leaks=0 applies
for the recorded LSan tracing fatal error. No global sanitizer settings change;
leaks remain unverified. No source changes follow the all-green invocation.

All argv, immediate exits, native symbol dumps, shared source SHA256/compile
commands, registration correspondence, results and full LastTest copy are saved
under ignored build/tilemap-u1-debug/u10-*, with the demo build log in its preserved
graphics output. Six Engine operations/diagnostic RTTI remain normal shared
exports/imports. Both compile databases use the exact same private editor source;
nm finds its consume in both binaries. Public consumer uses only Engine usage
requirements; no macro patch or implementation archive. Existing GNU PIE
RTTI/vtable COPY-relocation behavior and matching STL/toolchain requirements
remain; no installed SDK, Windows/other-toolchain/static/Release ABI is inferred.

## Limits and next action

No U10 CPU/build prerequisite remains. The ready committed execution receipt is
conditional on the observed checks above, not automatic source review or full
feature acceptance. Real input/display, app atlas decode/upload, capture,
graphical cleanup and leaks remain unverified. U4's probe exited 255 with
`Error: unable to open display :1`; no unchanged display probe/run is repeated.
No public guide/reference-generation campaign or hardware acceptance occurs.

This is the third and last pinned tilemap milestone in the finite batch, after
U8 79c02fb and U9 bdead05. Next action is coordinator source/batch review. A later
bounded owned-window/input/capture observation requires changed display access
and a separate assignment. No further worker unit, scheduler, sibling integration,
backend/storage/undo/chunking or recurring work starts here. Preserve all outputs
and checkpoints; publisher alone owns assigned-branch transport.
