# U8 Engine terrain owner checkpoint and next action

TILEMAP-U8 starts clean at accepted U7 checkpoint
`e97f6e84bf6a160261a89e22ccecb61eeaa4a5b9`. U7 established 61 unique passing
names through an initial 60/61 run plus an assertion-only correction and 1/1
rerun; it did not establish one green 61-case invocation. U8 promotes that
verified implementation, preserving all 61 regression names. Build, neutral
header compile, discovery and import/export inspection succeed. One accumulated
selection passes 64/64 (61 retained plus three new consumer checks), zero
failed/skipped, 37.21s, exit 0. U8 supplies CPU/shared-link evidence, not graphical
acceptance or automatic coordinator source review.

## Public caller and ownership

```cpp
#include <cheryl/maps/terrain-map.h> // first include; target links Cheryl::Engine
using CE::Tilemaps::TerrainLabel;
using CE::Tilemaps::TerrainMap;

CE::Tilemaps::Map base({3, 2}, 2);
const std::vector<TerrainLabel> labels{{1}, {1}, {0}, {1}, {2}, {0}};
// Caller-authored FourNeighbor rule returns targets 0..3; no asset policy here.
const std::array target_to_id{CE::Tilemaps::TileId{7}, CE::Tilemaps::TileId{8},
                             CE::Tilemaps::TileId{9}, CE::Tilemaps::TileId{10}};
TerrainMap terrain(base, {0}, labels, rule, target_to_id);
const auto affected = terrain.edit({1, 1}, TerrainLabel{1});
pass.add(CE::Rendering::Submission::resolve_tilemap_layer(
    terrain.tiles(), {0}, bindings, view, style, context));
```

The [public CPU header](../projects/engine/include/cheryl/maps/terrain-map.h) and
[Engine implementation](../projects/engine/src/maps/terrain-map.cpp) replace the
private unit producer/header. Namespace follows existing CE::Tilemaps; exact
spelling is a delegated engineering choice with low user-taste confidence.
Behavior/ownership confidence is high from the accepted prototype. Reversal now
costs public source migration and rebuild/relink, rather than only private edits.
No World, application semantic labels, backend, demo or JSON policy is exported.

The shared Engine owns construction, destruction, label/tiles queries and edit
operations through the existing export conventions. Existing invalid_args and
bad_request receive exported out-of-line destructors to anchor diagnostic RTTI;
explicit defaulted noexcept copy/move constructors retain their former implicit
semantics. Categories, exception data layout and terrain refusals are unchanged.
No consumer static/import macro patches or alternate terrain archive are used.
All eight pure and two retained-frame terrain cases now import this same Engine
implementation. The public owner remains neither copyable nor movable.

Inputs are stable only for construction, then copied: entire base Map, selected
layer, semantic labels, cardinal rule and dense target-to-logical table. Other
layers and original inputs stay independent. label returns an owned value;
tiles borrows a const live Map until owner destruction and reflects subsequent
edits. Serialize edits, reads and packet preparation. No temporary sampler,
mapping, event span or provider reference survives. Existing prepared frames
retain copied matrices/ranges/resources independently of owner destruction.
Binding metadata stability through every alias remains a coordinator-selected
engineering contract under delegation, not an explicit user API preference.

## Preserved contracts

X-right/Y-down cells; N=(0,-1), E=(1,0), S=(0,1), W=(-1,0). TerrainLabel is a
separate uint32 domain, zero empty; only equal nonzero semantic labels connect.
The existing Selection::select owns exact mask lookup. bit_order position owns
its bit; any ordered subset of 1..4 unique cardinal directions is supported.
Outside is Empty, never Unknown/wrap/clamp. EightNeighbor, diagonals, Wang,
empty/duplicate/unknown direction orders and out-of-domain mask keys reject
invalid_args before consumer storage copies. One rule/table applies to all
nonempty labels; no per-label asset policy is inferred.

Mapping length defines the selection-target domain. Target zero is valid;
zero logical entries are unmapped. Empty centers clear TileId{0} without lookup.
Needed missing cases, out-of-range targets (including SIZE_MAX), or unmapped
entries reject bad_request with coordinates. Unused holes are permitted. All
nonzero uint32 logical IDs remain legal here; retained binding/atlas checks stay
in the resolver. Logical ID 7 to original atlas target 0 still draws.

Construction checks nonempty/addressable extent, exact label count/storage
limits, selected layer and metadata before copying. Full initial derivation
prepares an owned layer before publishing the owner, preserving caller inputs.
Allocation failures propagate. Dense base/labels/initial scratch copy costs stay
unchanged; there is no huge allocation or OOM probe.

edit validates signed coordinates before neighbor arithmetic. Same labels return
an empty vector and do no resolution. Otherwise return owned unique center then
in-bounds N/E/S/W points, including empty neighbors; no diagonals. Resolve every
candidate using semantic labels plus the one-cell overlay before publishing any
label/visual edit. Bounds/rule/mapping/allocation failures preserve both complete
domains; other layers/maps and retained frames stay independent. At most five
cells/four samples each; this is single-edit atomicity, not frame/tick rollback.

## Focused consumer and verification

The [external header-first consumer](../projects/engine/tests/terrain-consumer/main.cpp)
is outside unit src. Its [CMake target](../projects/engine/tests/terrain-consumer/CMakeLists.txt)
links only Cheryl::Engine, inheriting language/includes/link requirements. The
existing neutral header-probe helper compiles the new header first and rejects
GL/GLFW leakage. It adds no unit-private includes or implementation copy, and
leaves CHERYL_BUILD_CONSUMER_TESTS OFF. It is registered only for shared Engine
builds, preserving static configurations without claiming to verify them.
Three explicit CTest modes check:

- values: short-lived/mutated constructor inputs, literal asymmetric masks,
  ordered edit/erase/no-op, second-owner and other-layer preservation, old copy.
- errors: typed casts/typeid across shared Engine, retained exception_ptr after
  input destruction, diagnostic copy/move and invalid-edit preservation.
- refusal: initially valid sparse rule; center/east resolve before south lacks
  mask 2, preserving every semantic/visual cell and the other layer/input map.

Existing 61 cases retain independent masks/full-grid edit oracles, all U1–U6
contracts, old/new frame ownership and zero synthetic native counters. U8 does
not rebuild demo: no compiled demo inputs change. No graphics work is attempted.

Preserved build/tilemap-u1-debug: Debug/shared Engine/tests/WARN/policy minimum
3.5, GNU 16.2.1, prior optional/native/demo/aggregate/acceptance/consumer OFF
selections and ASan/UBSan remain unchanged. Build only tests-engine plus the narrow
consumer/header dependency. All native work uses scripts/build and one job with
the normal shared queue, adaptive monitor caps, no swap and 4 GiB admission floor.
Command-local detect_leaks=0 remains the sole permitted LSan tracing workaround;
halt_on_error remains enabled for ASan/UBSan. Leak checking is unverified.

Exact executed commands (all argv, including formatter and native symbol
subcommands, are also saved in u8-commands.sh and u8-ownership.log):

```sh
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug \
  --target tests-engine tests-terrain-consumer --parallel 1
/mnt/projects/cheryl-features/scripts/build -- python3 build/tilemap-u1-debug/u8-check-ownership.py
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug -N \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.)'
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^(tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.|tests-terrain-consumer\.)'
/mnt/projects/cheryl-features/scripts/build -- readelf --relocs --wide --demangle \
  build/tilemap-u1-debug/projects/engine/tests/terrain-consumer/tests-terrain-consumer
```

| Operation | Immediate exit | Actual result / full local log |
| --- | --- | --- |
| Baseline exports / formatting | 0 / 0 | u8-baseline-exports.log / u8-format.log |
| Initial build | 1 | u8-build.log: new consumer called nonexistent Map::erase |
| Consumer setup correction | 0 | u8-build-correction.log: use set(..., TileId{0}); executables link; header probe compiles |
| Shared-only registration guard | 0 | u8-build-shared-guard.log: incremental regeneration/header probe |
| Initial ownership script | 1 | u8-ownership-initial.log: local parser split demangled names at spaces; all native nm/readelf calls exited 0 |
| Corrected ownership script | 0 | u8-ownership.log: exports/imports/link/sole producer checks pass |
| Registration | 0 | u8-registration.log: 64 unique names; all 61 accepted retained |
| Accumulated runtime selection | 0 | u8-ctest.log: 64 pass / 0 fail / 0 skip, 37.21s |
| Relocation inspection | 0 | u8-consumer-relocations.log: diagnostic COPY relocations explained below |

There is one U8 test invocation; no corrected-case rerun was needed. Initial
compile failure was only the new consumer setup spelling, corrected before any
U8 test execution; terrain algorithm/case bodies never change. The initial
rebuild emits seven dangling-else warnings in unchanged catalogue/font/runtime/
shader/math test sources, none in U8 inputs; correction/final builds emit none.
No sanitizer diagnostic occurs. Native-work queue waits keep their original
sessions; there is no admission/resource refusal, bypass or unchanged retry.

Registration matches source: 18 tilemap_consumer, 15 tilemap_submission, eight
tilemap_terrain, 20 tilemap cases (61), plus three external consumer modes.
The saved u8-registration-correspondence.json compares exact names to U7;
u8-results.json and u8-LastTest.log preserve actual counts/output. All logs,
full nm/readelf dumps, command record and ownership script are under ignored
build/tilemap-u1-debug, with no dependency/cache/global configuration changes.

Engine exports constructor/destructor/edit/label/tiles; the external consumer
object and unit executable import all five, with no test-owned implementation.
Engine exports both diagnostic destructors/RTTI; the consumer object imports
RTTI rather than emitting it. The normal GNU PIE executable has ELF COPY
relocations for that Engine-provided RTTI/vtables: its final nm shows V entries,
not a second source producer. Typed catches/typeid/dynamic casts/owned errors
execute successfully across the boundary. readelf records DT_NEEDED
libcheryl-engine.so.4; the saved Ninja link line (u8-consumer-link.txt) links only
that Engine library plus compiler/runtime requirements. Compile commands inherit
public includes/C++23/sanitizers and contain no private Engine/app paths or
CE_ENGINE_STATIC/CE_BUILDING_ENGINE consumer definitions. One Engine source owns
the terrain object; the link map and compile database contain no old unit copy.

The committed TILEMAP-U8 queue receipt names actual nonempty logs and reports
ready only after these checks. It is an execution receipt, not source review or
proof of any unexecuted platform.

## Limits and next action

The unchanged U4 X11 probe exited 255: `Error: unable to open display :1`.
No retry occurs. Actual input/display, app decode/upload, capture, visual terrain
edits, graphical teardown and leaks remain unverified. Public concrete STL types
require compatible compiler/STL/ABI and rebuilding/relinking Engine/consumers.
No installed SDK/export-manifest, Windows/other-toolchain or Release proof is
claimed. Public comments are source-reviewed; documentation generation is outside
this unit.

U8 has no unresolved CPU/build/link prerequisite. Next bounded action is
coordinator review of this public ownership checkpoint; the finite queue
controller may select its pinned U9 atomic terrain-edit batch only after this
run drains. That later unit must preserve the existing one-edit order and
transactional refusals; it is not implemented or started here. No demo
terrain control wiring, batch edits, dependency/pin changes, sibling integration,
publication or recurring schedule is part of U8. Publisher owns transport.
