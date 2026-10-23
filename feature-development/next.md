# U6 private editor checkpoint and next action

TILEMAP-U6 starts clean at coordinator-accepted U5 checkpoint
`ff76e5d6381243b05d2b450dd176d5b7a3dbc620` (44/44 Debug ASan/UBSan cases).
It adds one private CPU editor implementation shared by demo wiring and synthetic
consumers. CPU build/discovery/execution exit 0: **51/51 pass, zero skipped/failed,
32.16 seconds**, Debug ASan/UBSan. The demo also builds/links successfully
(exit 0). Real input/display remains unverified, with no unchanged X11 retry.

## Owners, caller and controls

[DemoTilemapEditor](../projects/apps/demo/src/tilemap-editor.h) owns two independent
3×2/two-layer CPU maps, selected map/layer, brush, pan/zoom and checked view data.
Its [implementation](../projects/apps/demo/src/tilemap-editor.cpp) handles real
ordered input records and prepares packets through existing APIs. The retained
atlas loader remains in [DemoTilemaps](../projects/apps/demo/src/tilemap-demo.cpp).
[GameImpl](../projects/apps/demo/src/demo-game.cpp) acquires Events capture before
tilemap mode's platform-init early return, initializes sizes there, then feeds
only copied tick data during simulation. Ordinary demo modes retain their paths.

```cpp
#include "tilemap-editor.h" // private app header; also first in consumer tests

DemoTilemapEditor editor;
editor.consume(tick.input.records(), {tick.logical_size, tick.framebuffer_size});
// Later, serially on the same preparation owner:
editor.write(frame, bindings, image_style);
```

No public engine header/API changes. The existing engine unit target explicitly
compiles this one private app CPU source and adds its include directory privately;
no demo/native module or separate runner is enabled for CPU tests. New consumer
cases call the exact helper used by the demo, rather than reimplementing its
state or pointer transforms. U5's earlier isolated camera consumer stays as a
regression, not the algorithm used by the new editor tests.

Controls are a coordinator-delegated engineering choice, not explicit user taste:

| Control | Effect |
| --- | --- |
| 1 / 2 | Select west / east map |
| G / T | Select ground / detail layer |
| 3 / 4 / 5 / 6 | Brush IDs 7 / 8 / 9 / 10 (brown / black / grey / red) |
| Left / right / middle Press | Paint / erase / sample selected layer, including empty zero |
| W / A / S / D Press | Pan up / left / down / right by 32 world units |
| Wheel Y | Zoom by exp2(delta/4), anchored at that record's pointer |
| R Press | Reset pan to zero and zoom to one, preserving maps/selection/brush |

Initial state is west/ground/brush 7. No held-button strokes, keyboard repeat
editing, undo or control-rebinding service. Only gameplay-admitted keyboard/mouse
records act, in supplied publication order. Each mouse button and scroll uses
its own optional captured pointer; a missing position ignores the operation.
Final Pointer/Axis samples cannot replace earlier positions. Release/Repeat and
other devices do not edit. Selection changes take effect for subsequent records
in the same tick. Earlier valid operations survive later ignored invalid events;
there is no tick/frame transaction promise.

## Coordinates, validation and lifetime

TickContext already carries logical and framebuffer sizes sampled together on
platform by GameRuntime::select_window, then copied into sequential/threaded
updates. GLFW callbacks record glfwGetCursorPos for EACH button/scroll. These
positions are observation-time logical top-left coordinates; sizes are the
selected tick sample, not a per-event resize history. A resize while records are
pending can thus use the later tick's dimensions. No simulation live-window
query, backend extension or historical reconstruction is added.

For logical Lw×Lh and framebuffer W×H, pointer q converts to camera-plane
`{qx*W/Lw, H-qy*H/Lh}` in full framebuffer viewport [0,W]×[0,H], Y-up. Axis
scales are independent. World is `pan + plane/zoom`; for map top-left O, local is
`{world.x-Ox, Oy-world.y}`. The existing pick_cell owns exact computed half-open
grid edges: top/left inclusive, internal edges choose the following cell, final
right/bottom exclusive. No epsilon or quotient index is introduced. Logical
window bounds are also half-open, so outside events ignore even if a map extends
there. Visibility derives from the same pan/zoom and framebuffer viewport.

Dimensions must all be positive. Pan/zoom/translation and projection/view matrices
must stay finite floats; zoom must remain positive after float conversion;
inverse visible rectangles for BOTH maps must be finite and noncollapsed.
set_view returns false before changing state for invalid candidates. Nonfinite
pointers/deltas or invalid/overflowed/underflowed wheel candidates ignore without
clamping. Invalid/minimized captured sizes suspend all events and emit no pass,
preserving maps, view, selection and brush; a later valid sample resumes them.
CPU math is double and render matrices float: arbitrary-world-ULP agreement with
GPU rasterization is not promised. No rotation, perspective or partial viewport.

Platform constructs/loads/captures before simulation begins. Update/preparation
run serially on the runtime's simulation owner; no concurrent writer/state view
or frame recycle is allowed. Helper calls retain no borrowed records, camera,
window, resource provider or frame spans. Deinit releases capture/helper on
platform after simulation joins and published frames finish; ordinary material/
manager/backend cleanup order remains. Prepared frames retain resource handles
and copied matrices independently of editor edits/destruction. Synthetic lifetime
checks are not native teardown acceptance. TM-002 metadata alias stability stays
an engineering contract, not an explicit user preference or runtime freeze.

Updated TM-003/004/005/006/009 contracts and stable TM-001–010 IDs in
[decisions.md](decisions.md) and [agent-notes.md](agent-notes.md) distinguish
contracts, source evidence and execution. Engineering confidence is high;
control/ergonomic preference confidence low. Reversal affects private app and
focused test sources only; no new public ABI. Coupling a private app source to
the existing engine consumer target is deliberate and reversible.

## Focused new coverage

Seven new tilemap_consumer cases retain all 44 prior names:

- editor_order: two Press clicks with different positions in one tick, final
  Pointer/Axis elsewhere, release/repeat filtering, separate cell edits and order.
- editor_selection: interleaved map/layer/brush selection, paint/sample/erase,
  sampling empty, gameplay/device filtering and missing positions.
- editor_edges: independent 2x/3x logical/framebuffer scaling, Y inversion,
  internal/outer computed edges and nextafter neighbors, half-open window bounds.
- editor_view: pan, release/repeat filtering, reset, distinct anchored wheel
  pointers, camera-derived exact-edge culling, copied matrices and old frame.
- editor_invalid: valid setup then invalid view candidates, overflow/collapse,
  nonfinite/missing/outside pointers/deltas and preserved maps/view/brush.
- editor_sizes: uninitialized/zero/negative logical/framebuffer dimensions,
  suspended events/preparation and restoration from a later positive sample.
- editor_frames: pick/paint/new-frame/erase/new-frame, complete sibling/other-layer
  isolation, 15/16/15 packet counts, authored order/detail alpha, retained old
  frames/resources and weak release after all frame/vector owners disappear.

All new tests use synthetic retained geometry/image/material resources and
assert zero native bind/draw/upload counters. No actual artwork or runtime startup
is needed. All seven cases execute successfully below; these assert CPU
contracts, not actual graphical input or display.

## Actual commands and results

Preserve `build/tilemap-u1-debug` (Debug shared Engine/tests/WARN ON, policy 3.5,
optional native/OpenGL/startup/UI/audio/workers/apps/docs/aggregate/acceptance/
consumer families OFF) and `build/tilemap-u4-display-debug` (accepted GLFW X11/
OpenGL/Startup/demo Debug selections). Incremental regeneration for the private
unit source/include and demo source glob keeps all selections/pins unchanged.
No fresh directory, install, download, global sanitizer or source ABI setting.

All configure/build/native checks run through scripts/build, one job, normal
shared queue and the current adaptive coordination/memory-policy.md limits:
4–20 GiB nominal shared cap, high one GiB lower, 8 GiB worker cap constrained by
parent, monitor-controlled admission, no swap, 4 GiB host admission floor.
Exit 75 means checkpoint without unchanged retry. Preserve complete logs and
immediate statuses under the existing clone-local ignored output directories.

Actual commands (all immediate exits 0):

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
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u4-display-debug --target demo --parallel 1
```

| Operation | Immediate exit | Observed result / full log |
| --- | --- | --- |
| Source formatting through wrapper | 0 | u6-format.log in CPU output |
| tests-engine incremental build | 0 | u6-build.log in CPU output |
| Focused registration | 0, 51 selected | u6-registration.log in CPU output |
| Focused execution | 0, 51 passed / 0 failed / 0 skipped | u6-ctest.log in CPU output |
| demo incremental build | 0 | u6-build.log in graphics output |

Registration matches source and executed names exactly: 16 tilemap_consumer
(#200–215), 15 tilemap_submission (#216–230), 20 tilemap (#231–250). All 44 U5
names are retained, with exactly seven editor additions. u6-results.json records
the immediate statuses/counts and correspondence; u6-LastTest.log preserves full
passing-case output. u6-commands.sh preserves exact commands, including formatting.
The helper compiles header-first with -Wall/-g/-fsanitize=address,undefined in both
outputs. No changed-source warning, assertion or sanitizer diagnostic occurs.
The demo build reports no source warnings. The unit target rebuild reports seven
pre-existing -Wdangling-else warnings in
unchanged asset-catalogue.cpp:229, font-selection.cpp:62, runtime-contract.cpp:429/
726/1246, shader-assets.cpp:468 and all-tests/src/math/math.cpp:371. They remain
outside U6 scope; no warning setting is weakened.

Leak checking remains unverified. Only command-local detect_leaks=0 addresses the
previous diagnosed `LeakSanitizer does not work under ptrace` fatal error; ASan/
UBSan and halt_on_error remain enabled, with global configuration unchanged.

## Graphical gap, limitations and next bounded action

No unchanged DISPLAY=:1 probe/run occurs. The saved U4 probe exited 255:

```text
Error: unable to open display :1
```

App Reader/decode/upload, actual display, real mouse/key input, screenshot/capture,
visual edit placement/order, graphical cleanup and leak checking stay unverified.
CPU helper events and a successful demo link supply no image or interaction
acceptance. Existing pins, prepared dependencies and all earlier logs/outputs
remain intact. No public API changes this unit; installed SDK/Windows/other
compiler/STL/GLM ABIs, Release and arbitrary-precision transforms remain outside
observed acceptance.

Next bounded action, requiring coordinator assignment: review this shared helper
and compiled wiring. Only after a changed display prerequisite, run one bounded
owned-window input/display observation for selection, paint/erase, pan/zoom and
cleanup. Do not retry unchanged X11, start another feature unit, add persistence/
resizing/chunking, integrate siblings or run a recurring loop. Commit coherent U6
source/tests/private wiring and these records, leave transport to publisher, stop.
