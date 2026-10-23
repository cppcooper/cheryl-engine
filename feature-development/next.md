# U5 CPU checkpoint and next action

TILEMAP-U5 starts clean from coordinator-accepted U4 source/build checkpoint
`6a85f160ffa3f3acd56312f707ebbb65bde5e607`. It implements checked CPU point
picking and a synthetic transformed pick/paint/erase consumer. Build, discovery
and tests exit 0: **44/44 pass, no skips, 39.52 seconds**, Debug ASan/UBSan.
No unchanged X11 probe or graphical operation runs. U4's atlas loading, display,
visual editing/capture and native cleanup remain unverified.

## Owners and small caller

Production changes are confined to [the CPU header](../projects/engine/include/cheryl/maps/tilemap.h)
and [implementation](../projects/engine/src/maps/tilemap.cpp): LocalPoint,
pick_cell and a checked footprint shared with visible_cells. [Point cases](../projects/engine/tests/unit/src/tilemap.cpp)
and [consumer cases](../projects/engine/tests/unit/src/tilemap-consumer.cpp) extend
the existing runner. The CPU header is included first by the core source and map
cases; it contains no assets/render/runtime/camera/GLM dependencies. Map layout,
get/set/copy/move methods and existing packet/binding/frame/pass/demo/input/
backend/startup/persistence APIs, build files and dependency pins are unchanged.
Matching TM-003/004/005/006 decisions describe geometry, conversion and editing in
[decisions.md](decisions.md) and [agent-notes.md](agent-notes.md), keeping TM-001–010.

```cpp
#include <cheryl/maps/tilemap.h>

namespace Maps = CE::Tilemaps;
Maps::Map map({3, 2}, 2);
const auto picked = Maps::pick_cell(map.extent(), {0.1, 0.1}, {0.15, 0.05});
if (picked)
    map.set({1}, *picked, {7}); // owned Point{1,0}; use TileId{0} to erase
```

The optional contains an owned signed Point, independent of map destruction or
later changes. A valid exterior point returns nullopt; invalid geometry or
nonfinite point coordinates throw invalid_args before any outside shortcut.
The shape follows existing exceptions/value queries; spelling and fixture names
are local engineering choices, not explicit user API tastes. Reversal cost is
small source migration/relinking for this new function, without a Map layout
change. Engineering confidence is high from source and independent cases;
personal-preference confidence remains low.

## Exact geometry and transformation contracts

Grid edge i is `double(i)*cell_size` on each axis, exactly as visible_cells.
Top/left are inclusive; an internal exact edge selects the following cell;
final right/bottom edges are exclusive. Both queries validate nonzero extent
axes, finite positive cell dimensions, finite positive footprint products and
increasing final cell edges before use. Binary upper-bound search selects the
first edge greater than an interior point, then subtracts one. No quotient,
epsilon, floating-to-integer conversion, camera ownership or valid-path
allocation is involved. Work/storage are O(log columns + log rows)/O(1);
geometry-only queries do not require addressable Map storage.

On the exercised binary64 platform, uint32 axes remain below 2^32 versus a
53-bit significand. Product rounding is too small to collapse a cell step;
subnormal sizes are positive integer multiples of denorm_min and remain valid.
With these bounded indices and finite products, interior/final collapsed grid
edges cannot be manufactured from valid sizes. Explicit positive/final-edge
checks remain defensive, and a caller size already rounded to zero rejects as
zero. This does not widen extent types or promise arbitrary floating-environment/
render precision. Invalid/overflow inputs are checked without OOM probes or
huge Map allocation; tests query maximum uint32 geometry directly.

The consumer-local ViewSample uses camera-plane point p in X-right/Y-up units:
`view=(world-pan)*zoom`, so world is `pan+p/zoom`. For map top-left world origin
O, local is `{world.x-O.x, O.y-world.y}`. A viewport W×H gives local bounds
`{pan.x-O.x, O.y-(pan.y+H/zoom), pan.x+W/zoom-O.x, O.y-pan.y}`.
Pass projection is orthographic [0,W]×[0,H], with matching zoom/translation view
matrices; identity model/unit style scale keeps the existing adapter contract.

Before mutation, the local consumer checks finite points/origins, positive
zoom/viewport values that stay positive finite floats, finite projection/view
matrices, finite inverse results and a positive-area transformed viewport.
A finite extreme pan can still collapse viewport edges and is rejected. The
helper does not clip supplied points to viewport bounds: map membership is the
query's job, and future input/UI gating remains separate. No camera framework,
real input, rotation, perspective or platform mouse-coordinate policy is added.
Ordinary double conversion and float render rounding apply; the tested pan
values/zooms 2 and 0.5 are exact, with no arbitrary-world-ULP guarantee.

TM-002 remains the coordinator-selected bind-once engineering contract under
delegated authority: source metadata stays unchanged through every alias while
any binding retains it. It is not an explicit user API preference or runtime
freeze. The synthetic fixtures construct const assets and retain resources;
per-call table/resource stability and borrowed writer/view lifetimes still apply.

## Meaningful new coverage

Five point cases join the 15 existing map cases:

- pick_caller: pick, paint/erase one layer and retain a coordinate after map death.
- pick_edges: asymmetric bounds, signed zero, outer misses/extremes, exact decimal
  43*0.1/3*0.1 edges and both nextafter directions.
- pick_invalid: zero extents, zero/negative/nonfinite sizes, nonfinite X/Y and
  overflowing footprints, including invalid geometry hidden behind exterior input.
- pick_limits: maximum uint32 axes, unit/decimal/minimum-subnormal/very-large
  sizes, exact last-cell/final edges and previous neighbors. No Map allocation or
  linear cell scan; visible_cells agrees on the last-cell rectangle.
- pick_oracle: independent half-open rectangle enumeration on 5×3, 3×2 and 1×4
  domains. More than 5,000 edge/neighbor/interior/exterior probes cover 0.1,
  0.3/0.7, thirds/sevenths, minimum subnormals/normals, tiny/huge values and
  footprints near double maximum. The oracle uses no production search/quotient.

Three consumer cases join the six accepted pass cases:

- pick_paint_erase: pan (90,170), zoom 2, viewport 460×48 converts (80,40) to
  world (130,190), local (30,10), west cell (1,0). Paint ground ID 10 into empty,
  prepare a new frame, erase via the same pick and prepare a third frame. Output
  counts are 6/7/6, with consecutive authored_order and the new ground before
  existing detail at that cell. Complete other-layer/sibling values and their
  packet payloads survive (assigned order can shift). Older/painted frames retain
  copied matrices/ranges/resources after erase and fixture destruction.
- pick_preservation: prove a valid pick before isolating ordinary negative/right/
  bottom misses and invalid zoom/pan/origin/point inputs, nonrepresentable matrices,
  inverse overflow and collapsed transformed viewport. Every operation preserves
  both maps and a previously prepared frame. Negative assertions surround the
  intended pick/edit operation; no renderer failure is substituted.
- pick_visibility: changed pan (120,174), zoom 0.5, viewport 120×12 picks the same
  cell with (5,8). Explicit local bounds {20,2,260,26} cull west column zero at its
  exact edge and include east cells. New packet order/parameters and retained
  older frame values are checked. No implicit sorting or Z rule appears.

Synthetic bind/draw/upload counters stay zero. Maps/bindings/provider die before
retained frames are inspected in the edit case. No writer/borrowed frame view
is observed concurrently with writing or recycling. These checks establish CPU
ownership and editing, not native-domain teardown or visual acceptance.

## Actual commands and results

Reused `build/tilemap-u1-debug` and its incremental outputs. Cache stays Debug,
GNU 16.2.1, policy minimum 3.5, shared Engine/tests/WARN ON. NativeGLFW, SDL3,
OpenGL, Startup, Workers, topology HWLOC/JSON, Linux debug terminal, TGUI/RmlUi,
audio Miniaudio, docs, SDK, demo/other apps, aggregate/acceptance/consumer test
families and CHERYL_STARTUP_OPENGL remain OFF. No reconfiguration, new directory,
runner, package download/install or pin change occurs. Build requests only
tests-engine and necessary dependencies. Existing source glob/GoogleTest discovery
registers the new cases; compilation retains -g/-Wall/-fsanitize=address,undefined.

All native work uses the existing shared wrapper/lock, one job, fixed 4 GiB
aggregate / 3 GiB throttling / 2 GiB worker / no-swap limits and 4 GiB host
admission floor. Live lock waits are ordinary queueing and are allowed to finish;
there is no exit-75 refusal or unchanged blocker retry. Exact commands executed:

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

| Operation | Immediate exit | Observed result / complete local diagnostic |
| --- | --- | --- |
| tests-engine incremental build | 0 | Engine/core + affected unit sources compile/link; u5-build.log |
| CTest registration | 0 | 44 nonempty entries; u5-registration.log |
| Focused execution | 0 | 44 pass, 0 fail, 0 skip, 39.52 seconds; u5-ctest.log |

All logs are under build/tilemap-u1-debug, with copied u5-LastTest.log and
u5-results.json preserving the immediate exits/counts. Registration corresponds
to nine tilemap_consumer cases (#200–208), 15 tilemap_submission (#209–223) and
20 tilemap (#224–243). All 36 saved U4 registered names remain selected, plus
exactly the eight additions above. Source formatting/whitespace, local record
links/fences/decision IDs and compact decision-word checks pass. No source
warning, assertion or sanitizer diagnostic occurs. No broad suite, display,
new native module configuration, documentation build or external agent is run.

Only invocation-local detect_leaks=0 is used for the previously diagnosed tracing
limitation; ASan/UBSan and halt_on_error remain enabled. Leak checking stays
unverified; no global settings change. Earlier diagnostic:

```text
LeakSanitizer has encountered a fatal error.
HINT: LeakSanitizer does not work under ptrace (strace, gdb, etc)
```

## Unchanged visual gap, limits and next bounded action

U4 source/build and its local dependency preparation/history stay preserved in
6a85f16, `build/tilemap-u4-display-debug` and u4-* logs. Its probe exited 255:

```text
Error: unable to open display :1
```

U5 does not retry that prerequisite, execute app atlas Reader/decode/upload,
launch the demo/capture helper or claim display, rendered edits, screenshots or
graphical cleanup. The earlier ignored owned-window check remains unexecuted.
CPU success is independent of that unchanged visual gap.

LocalPoint/pick_cell is source-additive; new callers must relink a compatible
Engine. Existing Map layout/methods are unchanged. GNU shared-library source use
is exercised, but installed SDK/Windows exports, other compiler/STL/GLM ABIs,
Release, other floating environments, arbitrary-scale precision, real input and
concurrent publication remain unverified. No stable cross-toolchain ABI is claimed.

Next proposed bounded unit, **requiring coordinator assignment**: wire the
accepted CPU pick/paint/erase and derived visibility into the private demo, with
selected map/layer and existing input/platform ownership. Keep production camera/
input/backend/startup/persistence APIs unchanged. Graphical observations wait
for changed X11 access and a bounded owned-window check. No such unit, display
retry, recurring schedule, sibling integration or transport starts in U5.
Commit only the four assigned source/test files and three records, preserve all
outputs/pins, leave assigned-branch transport to the coordinator publisher, stop.
