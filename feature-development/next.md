# U7 private cardinal terrain checkpoint and next action

TILEMAP-U7 starts clean at accepted U6 checkpoint
`fb0feefb62307138db88fa01a5488eb6d8885753` (51 accepted cases and compiled demo).
It implements a private CPU semantic-terrain consumer and transactional local
updates, independently of the interactive demo. Build/discovery succeed. The
61-name selection passes 60, fails one new fixture setup assertion, skips zero
(36.52s, exit 8). After correcting only that assertion, rebuild and focused rerun
succeed: 1/1 pass, zero failed/skipped (0.42s, exit 0). All 61 unique selected
names now have passing evidence; this is not a single 61/61 exit-0 run. All 51
accepted regressions ran once and passed. No display prerequisite is retried.

## Private owner and small caller

The [header](../projects/engine/tests/unit/src/tilemap-terrain.h) and
[implementation](../projects/engine/tests/unit/src/tilemap-terrain.cpp) live with
existing CPU consumer cases as an unshipped prototype. This is an engineering
choice with high contract confidence, low naming/taste confidence and local
reversal cost. No public Engine terrain, World, camera/input/backend, installed
header or reusable editor service is introduced. Existing source glob registers
the [eight pure cases](../projects/engine/tests/unit/src/tilemap-terrain-cases.cpp);
[two retained-frame callers](../projects/engine/tests/unit/src/tilemap-consumer.cpp)
reuse existing synthetic geometry/image/material fixtures. No CMake edit, new
runner or demo compiled input changes. The demo is not rebuilt in U7.

```cpp
#include "tilemap-terrain.h" // private consumer header
using namespace TilemapTerrain;

CE::Tilemaps::Map base({3, 2}, 2);
const std::vector<TerrainLabel> labels{{1}, {1}, {0}, {1}, {2}, {0}};
// Caller-provided Rule holds a FourNeighbor BitmaskRule; these cases return 0..3.
const std::array target_to_id{CE::Tilemaps::TileId{7}, CE::Tilemaps::TileId{8},
                             CE::Tilemaps::TileId{9}, CE::Tilemaps::TileId{10}};
TerrainMap terrain(base, {0}, labels, rule, target_to_id);
const auto affected = terrain.edit({1, 1}, TerrainLabel{1});
pass.add(CE::Rendering::Submission::resolve_tilemap_layer(
    terrain.tiles(), {0}, bindings, view, style, context));
```

The caller supplies stable inputs during construction. TerrainMap copies the
entire base Map, selected layer, row-major labels, bitmask metadata and dense
mapping; temporaries may die afterward. It derives only the chosen layer,
preserving copied other layers and the original base. TerrainLabel is a separate
uint32 domain: zero empty, all other values opaque equality labels. No semantic
meaning is inferred from sprites or logical IDs. One rule/mapping applies to all
nonempty labels in this first consumer; per-label palettes remain deferred.

TerrainMap is deliberately neither copyable nor movable; label(Point) returns
an owned checked value and tiles() borrows its const live Map. Serialize edits,
reads and frame preparation. No references to temporary samplers, mappings,
event spans, windows or providers survive calls. Prepared frames own copied
packets/resources; observe them only outside writer/recycle scopes. Existing
TM-002 binding metadata alias stability remains a coordinator-selected engineering
contract, not an explicit user preference or runtime freeze.

## Exact derivation and edit contract

Cells are X-right/Y-down. North=(0,-1), East=(1,0), South=(0,1), West=(-1,0).
Metadata bit_order position owns the bit; N/E/S/W means weights 1/2/4/8.
W/S/N/E instead means N/E/S/W weights 4/8/2/1. Compatible ordered subsets of
1–4 unique cardinal directions are accepted; omitted directions create no bit.
The existing Selection::select(BitmaskRule) owns sampling/connectivity lookup;
its direction/type validation is reused even for an all-empty semantic grid.
Out-of-domain case mask keys reject invalid_args. EightNeighbor, diagonals, Wang,
empty/duplicate/unknown direction orders reject before consumer storage copies.

Only same nonzero semantic labels connect. Samples are Known in bounds and
Outside beyond the finite map, with fixed Empty fallback. No Unknown, wrap,
clamp, chunk or unloaded-cell model. Empty centers directly derive TileId{0}
without consulting a case/table; nonempty centers require an exact mask case.
Selected targets are size_t ordinals indexing the copied target_to_id table;
its length defines the accepted target domain, including valid target zero.
A zero table entry is unmapped, never a hidden drawable nonempty terrain.
A used missing case, out-of-range target (including SIZE_MAX) or unmapped entry
throws bad_request with cell/label or target coordinates. Missing cases/holes
may remain unused; an entirely empty grid may have empty cases/table. Every
nonzero uint32 logical ID is legal here; retained bindings and actual atlas
validation stay separate. Synthetic ID 7 binds original atlas target 0 and draws;
empty semantic zero produces logical zero and no packet. No asset is selected or
animation advanced by this helper; the existing adapter resolves original targets
once at its own elapsed time.

Construction checks nonzero base dimensions, size_t products, vector max_size,
exact label count, layer bounds and rule shape before copying consumer storage.
The base already owns validated finite dense Map storage. A moved-from base
rejects; no large allocation or OOM probe is used. Full derivation builds a
complete owned layer result before assigning internal visuals/returning the
object; caller inputs cannot be partially edited. Allocation failures propagate.
Cost includes a full base copy, one semantic layer, initial layer scratch and
copied rule/table; there is no new application cell-budget API.

edit validates signed coordinates BEFORE neighbor arithmetic. Valid uint32-bounded
points plus cardinal +/-1 fit int64. Same-label edits return an empty vector and
do no resolution. Otherwise return unique invalidations in center, then in-bounds
N/E/S/W order, including empty neighbors: one through five cells, no diagonals.
Each result samples semantic labels with the proposed one-cell change overlaid,
never cached visual IDs. All results and return storage exist before one label
and the fixed-layer visual cells are assigned without allocation. Invalid bounds,
missing cases/mappings and pre-commit allocation failures preserve both complete
domains. No external mutable Map access can invalidate the layer/extent invariant.
This is a single-edit transaction, not a whole-frame/tick rollback guarantee.
Edit work/storage is bounded to five cells/four samples each, independent of map
area; construction and the independent test oracle enumerate the full grid.

TM-007 and TM-006 in [decisions.md](decisions.md) and
[agent-notes.md](agent-notes.md) retain matching IDs, alternatives, confidence,
ownership and this operation's distinction from the U6 event tick contract.
All U6 event-pointer, size/Y/viewport, pan/zoom, selection and frame contracts are
unchanged. No private interactive terrain binding, rule replacement, Wang model,
persistence, physics, undo, procedural/sibling import or chunking is selected.

## Meaningful evidence

Eight tilemap_terrain cases:

- masks: literal N/E/S/W masks for 3x3 isolated/corner/edge/interior and asymmetric
  3x2 grids, independent of the production selector formula.
- bit_order: literal W/S/N/E expectations and compatible E/N subset semantics.
- semantics: different labels sharing a visual ID must not connect; one label
  with three different visual IDs must connect. Arbitrary nonzero labels work.
- invalidation: exact center/corner/edge lists, no duplicates/diagonals; other
  layer intact and unchanged-label no-op.
- sequences: 384 edits on 5x3, 1x4, 4x1, 1x1 under canonical/reordered metadata.
  Independent full-grid boolean-neighbor/mask oracle calls neither the helper nor
  select. Independent whole-grid Manhattan enumeration checks invalidations.
- failures: initially valid sparse rules; center resolves empty before a later
  neighbor needs a missing mask, out-of-range target or unmapped entry. Complete
  label/visual snapshots survive; valid repaired inputs prove the edit succeeds.
  Negative/right/bottom/INT64_MIN/MAX points reject before arithmetic.
- metadata: owned inputs survive caller mutations; invalid label count/layers,
  EightNeighbor/Wang/diagonal/duplicate/unknown directions, mask keys, SIZE_MAX
  targets and moved-from bases reject safely.
- empty: minimal/outside empty fallback, no wrap/center substitution, erase,
  no-op, absent unused metadata, unmapped used entries, arbitrary logical IDs.

Two additional tilemap_consumer cases:

- terrain_frames: two independent terrain owners, two layers, synthetic retained
  bindings/pass writer. One semantic edit recomputes target ranges, producing
  9 then 8 ordered packets; other layer/map payloads stay unchanged apart from
  assigned order. Older frames preserve matrices/ranges/resources after edits
  and all owners die. Weak observations prove release after last frame/vector.
- terrain_rejection: valid initial full 3x2 terrain on both maps produces 15
  packets. After center and east candidates resolve, south refuses a missing
  mask or SIZE_MAX target; labels/tiles and old/new prepared output stay exact.
  Initial assertion incorrectly expected 11; correction to 15 reaches both
  intended refusal paths and passes. Helper implementation did not change.

Native bind/draw/upload counters stay zero. No assertion implies actual image,
input, atlas upload, native teardown or visual acceptance.

## Actual commands and results

Preserved build/tilemap-u1-debug: GNU 16.2.1 Debug/shared Engine/tests/WARN ON,
policy minimum 3.5, prior optional native/OpenGL/startup/UI/audio/workers/apps/
docs/aggregate/acceptance/consumer selections OFF. Only tests-engine/dependencies
build; private source-glob regeneration changes no cache selection or pin. Helper
and cases compile with -Wall/-g/-fsanitize=address,undefined. All work uses the
shared wrapper/normal queue, one job, current adaptive memory policy/monitor,
8 GiB worker ceiling constrained by shared parent, no swap and 4 GiB host floor.
There is no resource refusal or unchanged retry. No demo build/run occurs.

Exact native commands (format argv is also preserved in u7-commands.sh):

```sh
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug --target tests-engine --parallel 1
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug -N \
  -R '^tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.'
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug --output-on-failure --no-tests=error \
  -R '^tests-engine\.(tilemap|tilemap_submission|tilemap_consumer|tilemap_terrain)\.'
# After correcting only the new fixture's setup-count assertion:
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build build/tilemap-u1-debug --target tests-engine --parallel 1
ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/tilemap-u1-debug --output-on-failure --no-tests=error \
  -R '^tests-engine\.tilemap_consumer\.terrain_rejection$'
```

| Operation | Immediate exit | Actual result / full local log |
| --- | --- | --- |
| Formatting via wrapper | 0 | u7-format.log |
| Initial build | 0 | u7-build.log, no source warnings |
| Registration | 0 | 61 nonempty names; u7-registration.log |
| Initial full selection | 8 | 60 pass / 1 fail / 0 skip, 36.52s; u7-ctest.log |
| Assertion-correction build | 0 | u7-build-correction.log, no warnings |
| Corrected negative consumer | 0 | 1 pass / 0 fail / 0 skip, 0.42s; u7-ctest-correction.log |

Logs/commands/results live under ignored clone-local build/tilemap-u1-debug,
including saved u7-LastTest-initial.log and u7-LastTest-correction.log. Registration
is 18 consumer (#200–217), 15 submission (#218–232), eight terrain (#233–240),
20 map (#241–260). All 51 accepted names remain selected and pass in the initial
run. Final unique passing coverage is 61; no whole-suite exit-0 rerun is claimed.
No sanitizer diagnostic occurs; ASan/UBSan/halt_on_error stay enabled. Only
command-local detect_leaks=0 is allowed for the diagnosed LSan tracing fatal
error; leak checking remains unverified, with no global setting change.

## Limits and next action

The unchanged U4 X11 probe exited 255: `Error: unable to open display :1`.
No retry, desktop access or native loop occurs. Actual graphical input/display,
app asset decode/upload, capture, visual autotile editing, graphical teardown,
leaks, installed SDK/Windows/other-toolchain ABI and Release remain unverified.
There is no new public ABI: the private prototype is compiled only by unit cases.

Next bounded action requires coordinator selection: review the private semantic/
mapping and per-edit publication boundary. If accepted, a later assignment can
choose whether to integrate terrain editing into the private tested editor or
promote a minimal reusable owner. Do not activate either now. Any graphical
observation requires changed display access and a bounded owned-window check;
no unchanged retry, new assets/pins, sibling integration or recurring schedule.
Preserve all outputs and checkpoints, leave transport to publisher, stop after U7.
