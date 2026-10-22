# First tilemap milestone: proposed contracts

This is a branch-local design record for the next feature worker. No tilemap
implementation or executable acceptance is claimed. The reader summary is
[decisions.md](decisions.md); the single next unit is [next.md](next.md).
Decision IDs TM-001 through TM-010 identify the same choices in both views.

The inspected baseline is `3542f883391d4225329dab8d5d61a5987e69c8f0` on
`feat/tilemap`. Initial index and working tree were clean, with no pre-existing
work to extend. This investigation owns only this clone. Workspace authority is
the parent `AGENTS.md`, `coordination/autonomy-policy.md`, and the tilemap brief
in `coordination/feature-briefs.md`; the explicit assignment stops after these
records and a local commit. Source inspection and record checks are the evidence
for this phase. Builds, dependency retrieval, native tests, visual acceptance,
published documentation, and feature implementation remain unperformed.

The coordinator resolved the read-only `.git` runner conflict that blocked the
earlier local checkpoint.
Record review checked matching decision IDs, decision lengths, local links and
the cited source paths. This is static evidence only.

## Proposed caller sequence

All `Tilemaps`, `TilemapBindings`, `TilemapView`, and `resolve_tilemap_layer`
names below are proposals, not existing APIs or compiled examples. `FrameWriter`,
`Context2D`, and asset handles follow the current engine. Exact new naming has low
preference confidence and should stay local to one consumer until exercised.

```cpp
namespace Maps = CE::Tilemaps;                    // proposed CPU domain
namespace Submit = CE::Rendering::Submission;    // existing submission domain
constexpr Maps::LayerIndex ground{0}, detail{1};
constexpr Maps::TileId empty{0}, floor{1}, marker{2}, animated{3};

// init_platform(): obtain and validate a retained, uploaded tileset first.
// 'overworld' is a checked shared_ptr<const Assets::Tilesets::Asset>.
// Follow the current demo's manifest -> images -> tileset -> get_asset sequence.
Submit::TilemapBindings bindings;
bindings.add(floor, overworld, CE::Assets::Grids::CellIndex{0});
bindings.add(marker, overworld, CE::Assets::Grids::CellIndex{1});
bindings.add(animated, overworld, CE::Assets::Grids::CellIndex{309});

// Consumer-owned CPU state: no renderer, loader, or level needed to create it.
Maps::Map west({8, 6}, 2); // extent {columns, rows}, layer count
Maps::Map east({8, 6}, 2);
west.set(ground, {2, 1}, floor);  // point {column, row}
west.set(detail, {2, 1}, marker);
east.set(ground, {2, 1}, animated);

// update(): apply input on the simulation owner, including erase and clocks.
west.set(detail, {2, 1}, empty);
// Advance west_elapsed and east_elapsed from simulation delta, even if no frame
// is prepared. They are separate consumer-owned nonnegative millisecond clocks.

// prepare_render_frame(): frame, projection, camera_view, and clocks supplied by
// the existing runtime/consumer. Rectangles are map-local, X-right/Y-down units.
auto pass = frame.begin_pass(projection, camera_view);
const Submit::Context2D context{
    pass.semantics(), pass.parameters(), pass.constraints()
}; // existing automatic image binding remains enabled
const CE::Rendering::DrawStyle2D style{}; // asset material, identity model, scale 1
const Submit::TilemapView west_view{
    .origin = {16, 240}, .cell_size = {16, 16},
    .visible = {0, 0, 128, 96}, .elapsed = west_elapsed
};
const Submit::TilemapView east_view{
    .origin = {176, 240}, .cell_size = {16, 16},
    .visible = {0, 0, 128, 96}, .elapsed = east_elapsed
};
// These visible rectangles show each entire map; derive smaller rectangles from
// the camera when panning. Map origins are top-left positions in world Y-up units.
pass.add(Submit::resolve_tilemap_layer(west, ground, bindings, west_view, style, context));
pass.add(Submit::resolve_tilemap_layer(west, detail, bindings, west_view, style, context));
pass.add(Submit::resolve_tilemap_layer(east, ground, bindings, east_view, style, context));
pass.add(Submit::resolve_tilemap_layer(east, detail, bindings, east_view, style, context));
// The application can insert actor draws between layers or suppress either map.
```

These are lifecycle fragments, not one function: CPU objects/bindings are retained
consumer members. Maps may be destroyed after CPU resolution without invalidating
packets. Stop simulation, let the existing runtime finish/recycle frames, and
release retained asset bindings during platform cleanup before backend teardown
(S7, S12). CPU-only maps need none of that graphics lifecycle.

## TM-001

**Own finite map values separately from resources.** `CE::Tilemaps::Map` owns one
dense, layer-major, row-major `TileId` vector; its index is
`(layer * rows + row) * columns + column`. Dimensions and layer count are fixed
after construction. Construction initializes every cell to empty, requires
nonzero axes/layers, and checks every count multiplication and `vector::max_size`
before allocating. Invalid dimensions/storage counts throw `invalid_args`;
allocation failure propagates. Copying gives independent cells; no mutable cell
references or renderer pointers escape.

One simulation owner mutates a map and reads it during frame preparation; no
concurrent mutation, retained cell view, background jobs, or synchronization API
is introduced. Construction/query/edit work in a CPU consumer with no manager,
active game, or native backend. Alternatives: store world data inside the shared
tileset, or introduce an ECS/level manager. Both confuse asset versus instance
ownership and contradict independence. Dense allocation costs O(layers × width ×
height) and defers sparse/streamed worlds. Confidence: high from explicit scope
and existing asset/packet separation (S1–S3, S7); historical taste evidence is weak
(H1, H4). Later chunking replaces storage behind value queries rather than
requiring callers to hold vector addresses.

## TM-002

**Separate logical tile IDs, atlas cells, and retained bindings.** A proposed
strong `TileId` wraps `uint32_t`; zero denotes empty and never submits a draw.
Atlas cell zero remains an ordinary drawable cell. Nonzero IDs are application
identities within a palette domain, not global asset IDs, terrain labels, vertex
offsets, or animation frames. `Map::get` returns a `TileId` by value. The CPU map
accepts every representable ID; it cannot validate a renderer's palette.

Setup-owned `Submission::TilemapBindings` maps an ID to a retained
`shared_ptr<const Tilesets::Asset>` plus an original `Grids::CellIndex`. `add`
rejects zero IDs, duplicate bindings, null assets, and out-of-grid targets with
`invalid_args`, preserving existing bindings. Bindings remain stable during
resolution; there is no hot replacement API in this milestone. Missing nonzero
bindings fail resolution with the logical ID and map location; no silent blank
fallback. The consumer paints only registered IDs.

Alternative direct atlas ordinals in cells are smaller conceptually but attach
map meaning to an atlas layout and cannot distinguish empty from cell zero.
Per-cell `Tile` handles duplicate ownership without a logical identity. A string
key per cell incurs avoidable allocation/storage. Confidence: medium engineering
choice, low preference inference (H4 contains both values and mutable selection).
Future persistence must define palette names/version mapping and record qualified
asset identities plus original targets, not serialize shared pointers or assume
`size_t`/these integers are an agreed file format (TM-010).

## TM-003

**Use checked integer map coordinates and an explicit placement conversion.**
`Extent` is `{columns, rows}` with `uint32_t` axes; `LayerIndex` is a distinct
zero-based index. `Point` uses signed 64-bit `{column, row}` so negative input
is checked before any unsigned cast. Legal cells satisfy
`0 <= column < columns`, `0 <= row < rows`, and `layer < layer_count`.
`get`/`set` throw `bad_request` for invalid points/layers without changing cells;
there is no wrap or clamp on edits. The constructor's checked storage count makes
the subsequent linear index representable.

Rows increase south/down, columns east/right. `CellSize{width,height}` supplies
finite positive map-local units (16×16 in the fixture), independent of the source
image's cell dimensions. The map-local footprint is
`[0, columns*width) × [0, rows*height)`; all products used for placement must remain
finite. A view's world origin `O` is the map's top-left, with world Y increasing
up. Cell `(x,y)` spans world X `[Ox+x*w, Ox+(x+1)*w)` and world Y between
`Oy-(y+1)*h` and `Oy-y*h`.

Require positive source frame dimensions and a finite normalized source pivot
before division, including directly constructed assets that bypass loading.
For source frame size `(fw,fh)` and top-left pivot `(px,py)`, place its pivot at
`(Ox+(x+px)*w, Oy-(y+py)*h)`, then scale source geometry by `(w/fw,h/fh)`.
This preserves the source's Y-up geometry without vertically flipping artwork
(S5). Reject origins, scales or resulting transforms that cannot be represented
as finite floats in the current rendering matrices. The first adapter requires
identity base model and `style.scale == 1`;
placement owns translation/size and rejects other transforms rather than letting
culling disagree. It preserves alpha, material override, custom parameters, and
clip inputs. Arbitrary map rotation, oversized cell overhang, and isometric maps
are outside this milestone. Confidence: medium, a reversible map convention,
not an inferred user preference. Y-up map rows were viable; Y-down rows make
row-major authoring and future neighbor offsets direct.

## TM-004

**Select positive-area visible intersections using half-open bounds.** A proposed
pure CPU helper `visible_cells(extent, cell_size, LocalRect)` returns a copied
cell range `[first_column,end_column) × [first_row,end_row)`. `LocalRect` is
continuous `{left,top,right,bottom}` in map-local Y-down units. Validate nonzero
extent axes, finite edges, ordered axes, finite positive cell size and finite
footprint products first. Invalid extents, inverted/NaN/infinite inputs or
overflowing footprint products throw `invalid_args`; zero-area input and a
rectangle merely touching or outside the map return an empty range.

Intersect with the map footprint before dividing or converting to integers.
For a positive intersection, begin is `floor(min/size)` and exclusive end is
`ceil(max/size)`, clamped to each dimension. Intersecting first prevents negative
coordinates from becoming huge unsigned indices and prevents huge external
rectangles from overflowing casts. Do not add an epsilon that includes a tile
lying entirely beyond an exact boundary. Use double precision for these helper
inputs/intermediates; this does not promise arbitrary-scale rendering precision.

For an 8×6 map with 16-unit cells, expected ranges include:

| Local rectangle | Columns | Rows |
| --- | --- | --- |
| `[0,0,16,16)` | `[0,1)` | `[0,1)` |
| `[16,0,32,16)` | `[1,2)` | `[0,1)` |
| `[15.5,0,16.5,16)` | `[0,2)` | `[0,1)` |
| `[-8,-8,8,8)` | `[0,1)` | `[0,1)` |
| `[112,80,128,96)` | `[7,8)` | `[5,6)` |
| `[128,0,144,16)` or `[0,0,0,16)` | empty | empty |

Given an axis-aligned world view with left/right/bottom/top edges `(L,R,B,T)`,
the consumer supplies local rectangle `(L-Ox, Oy-T, R-Ox, Oy-B)` per instance.
The same inverse conversion followed by `floor` maps a world editor point to a
cell; exact right/bottom map edges are outside. Camera translation/positive zoom
are consumer responsibilities. Selection is culling, not a scissor: a partially
visible cell is emitted whole and the existing viewport/optional clip cuts pixels.

Alternatives are scanning every cell against `Math::overlaps`, an inclusive
maximum index, or having the map inspect a live camera. The existing positive-area
overlap helper (S11) is useful as an independent test oracle, but a computed range
avoids O(total cells) traversal and camera coupling. Confidence: high engineering
confidence; coordinate preference remains medium. No implementation or boundary
test has run yet.

## TM-005

**Let authored call order determine layer order.** The first consumer requests
two layers and names index 0 ground and index 1 detail. Rendering one layer
traverses the visible range by increasing row, then column. The caller explicitly
submits ground before detail; it may insert actors, reorder layers, or hide one.
There is no sorting by texture, logical ID, Z value, or unordered container order.
Two instances' relative order is likewise the caller's sequence. Layer count is
fixed; names, visibility and editor selection belong to the consumer.

Default 2D pass constraints and ordinary textured materials disable depth;
straight-alpha composition makes later detail draws visible over earlier ground
draws (S7–S9). A custom material must match the pass and cannot rely on a hidden
depth override. Alternatives include implicit layer Z or automatic material
batching. Those introduce competing ordering policies. Confidence: high because
the brief explicitly requires authored order and current playback preserves it;
there is no v3 evidence for a preferred layer API.

## TM-006

**Apply edits synchronously and resolve from current state.** A valid `set` changes
one cell in place, including erase to ID zero. There is no GPU upload, dirty
notification, mesh cache, or deferred edit queue. The next prepared frame observes
all preceding owner-thread edits; packets already prepared keep their old values.
Setting the same value is harmless. Invalid edits preserve the map. Consumer
selection/highlights are separate state, not a third logical layer.

Each visible nonempty binding supplies its original target to
`tileset.cell_at(target, view.elapsed)` before packet resolution. Each instance's
nonnegative elapsed milliseconds are advanced by update, even when the runtime
skips preparation. No tilemap-owned thread, clock, or animation cursor is needed.
The shared tileset is never mutated. Editing map A or changing its clock cannot
change map B (S3, S12). Alternatives: bake frames into map cells, or rebuild a mesh
after each edit. These complicate identity and stale state. Confidence: high for
existing playback compatibility, medium for the intentionally simple recompute
strategy. A cache later needs explicit invalidation and animation handling.

## TM-007

**Begin with authored tiles; preserve the selector boundary.** Initial cells
store explicit IDs bound to original atlas targets. Automatic terrain connection
and Wang editing are deferred, not silently inferred from tile ID equality.
The selector supplies no map storage, camera or layer model (S4). Its callback is
invoked synchronously and is not retained; `Outside` differs from `Unknown`, and
`MissingRule` differs from `IncompleteNeighborhood`.

A later bitmask adapter can sample same-layer terrain at N `(0,-1)`, E `(1,0)`,
S `(0,1)`, W `(-1,0)`, returning `Outside` beyond this finite map. Terrain IDs need
their own definitions; Wang shared edges/corners cannot be fabricated as ordinary
neighbor cells. A deterministic seed should derive from stable local coordinates
and an authored seed, never camera visibility or traversal position. Keep these
as extension constraints, not new APIs in this milestone.

If selection is later used, either call `select_tile(...,elapsed)` and submit its
final cell, or call CPU `Selection::select`, then `cell_at` once. Do not apply
animation a second time to `select_tile`'s result (S3, S10). Adding all selector
modes now would require extra terrain authoring and edit invalidation unrelated
to acceptance. Confidence: high compatibility evidence, medium scope choice;
no deliberate v3 map example resolves the preference.

## TM-008

**Resolve one retained packet per visible nonempty cell through existing owners.**
The proposed adapter is a free CPU function in `Rendering::Submission`, separate
from CPU map storage. It borrows stable map/bindings/view/style/context for one
call, computes the visible range, resolves target animation and pivot placement,
then calls existing `resolve_tile(asset, final_cell, style, context)` (S6). It
returns an owned ordered vector of packets; there are no native binds, draws,
uploads, retained map references, or live camera references.

Validate the layer, view (including nonnegative elapsed), base transform and
binding metadata before use; source material/geometry/range validation remains
authoritative. Unbound visible IDs, bad view inputs, incompatible resources or
allocation failures return no vector. Use `pass.add(vector)` for the whole layer:
current group validation/reservation leaves earlier draws intact on failure (S7).
Earlier successfully added layers remain in the private frame; no whole-map
transaction is promised. Let runtime preparation failure discard/recycle that
frame rather than publish a partial result. Offscreen IDs need no resolution.

Bindings retain assets; packets independently retain geometry, material/pipeline
and images, with copied matrices. They do not keep the map, binding table or asset
metadata alive. Native resource-domain validity still depends on the original
live backend. Existing runtime graphics-owner recycling/cleanup remains required,
even though resolution is CPU-only (S7, S8, S12).

For V visible cells and N nonempty cells on a layer, traversal/resolution costs
O(V) with expected constant-time binding lookup, output storage O(N), and current
playback performs one pipeline invocation per packet (up to N native draws;
an empty clip can suppress drawing). No batching or performance target is
claimed. An all-filled 8×6 two-layer map emits
96 packets; two such maps emit 192. Temporary vectors and repeated packet
validation are a conscious bounded cost. Do not concatenate atlas strips into
one strip: it connects unrelated geometry and lacks per-cell transforms (S5).
Confidence: high compatibility; medium cost choice. Reuse output storage or chunk
meshes only after measurement, retaining order and animation semantics.

## TM-009

**Use one small independent editor consumer with two instances.** Extend the
existing demo via a dedicated tilemap sample helper, rather than inventing a level
loader or requiring an optional UI module. Keep two 8×6 maps, two layers each,
16-unit cells, separated world origins, and independent edit/visibility/time
state. Paint/erase a selected cell/layer; switch active instance; toggle either
instance and layer; pan across boundary positions. The fixture is authored in
memory on startup and may be reset in memory. No map files are read or written.

Reuse current image/tileset loading and the existing runtime/input/render path
(S9, S12). For the art sample, load the existing Puny World manifest, find entry
`overworld`, and retrieve its actual qualified `definition.id()`; do not guess its
namespace. Source demo requests cells 0 and 1 and animated target 309. The assets
submodule is uninitialized here, so those manifest contents/artwork were not
independently inspected. Validate required cells/clips and report an unavailable
fixture explicitly; an optional-demo skip does not count as acceptance.

Unit acceptance uses synthetic retained geometry/image/material fixtures from
S10 and needs no installed image files or native backend. Later visual acceptance
does need a feature-local asset checkout and compatible renderer. Neither is a
reason to alter dependency pins or retrieve anything during this design phase.
Alternative a standalone application is possible but adds target/setup work;
the existing demo supplies the lifecycle already. Confidence: high explicit
consumer requirements, medium scaffold choice; exact editor action bindings will
be chosen against the demo's active input contexts in its implementation unit.

## TM-010

**Keep integration seams explicit and independent.** Do not merge siblings or
require settings, map storage, level lifecycle, physics, procedural generation,
or an ECS. No dependency additions are proposed. CPU map headers must not depend
on graphics resources or runtime headers; the submission adapter points toward
both maps and existing assets/rendering, never the reverse. Existing tileset,
selector, packet, material, frame and runtime public contracts remain compatible.

| Overlap | Contract to carry forward; no prerequisite |
| --- | --- |
| Map storage | Agree later on palette domains, empty ID, row/layer order, asset/target identity and integer widths. Do not assume a sibling binary format or serialize memory layout. |
| Settings/input | Consumer actions use existing input contexts. No settings persistence or rebinding feature is needed; avoid editing another branch's contracts. |
| Rendering | Retained generations, pivot transforms, straight-alpha authored order, clips, and owner-thread recycling. Any required backend fix stays local and needs a separately assigned unit. |
| Storytelling/lifecycle | A map instance is an ordinary value; no global active map, session transition, spawn API or entity registry. |
| Chunking/streaming | Future storage can preserve checked point queries. Unknown regions and neighbor invalidation require a later contract; this finite map has no unloaded cells. |

Alternative shared cross-feature infrastructure would settle speculative contracts
before a small consumer exists. Confidence: high from assignment and workspace
policy. Surface overlaps to the coordinator through handoff; edit only this clone.

## Source evidence at the inspected baseline

These are source observations, not executed acceptance. Line spans refer to
`3542f883391d4225329dab8d5d61a5987e69c8f0`; links resolve in this clone.

| ID | Owner and concrete evidence |
| --- | --- |
| S1 | [Grid definition](../projects/engine/include/cheryl/assets/definitions/grid.h), lines 11–41; [implementation](../projects/engine/src/assets/definitions/grid.cpp), lines 6–31. Atlas row-major addressing, count overflow and cell rejection; no world coordinates. [Tileset definition](../projects/engine/include/cheryl/assets/definitions/tileset.h), lines 87–101, owns metadata and qualified identity. |
| S2 | [Tileset manager](../projects/engine/src/core/resources/asset-management/tileset-mgr.cpp), lines 15–60; [declaration](../projects/engine/include/cheryl/core/resources/asset-management/tileset-mgr.h), lines 18–26. Image first, retained material, one geometry upload, publish absent IDs; existing IDs are retained and partial loading can survive failure. |
| S3 | [Tileset asset](../projects/engine/include/cheryl/assets/types/2d/tileset.h), lines 19–98; [implementation](../projects/engine/src/assets/types/2d/tileset.cpp), lines 27–97. Owns definitions/resources; checked static `tile`, independent animation values, nonnegative `cell_at`, selection then one animation substitution, borrowed metadata. |
| S4 | [Selector declaration](../projects/engine/include/cheryl/assets/selection/tile-selection.h), lines 18–79; [implementation](../projects/engine/src/assets/selection/tile-selection.cpp), lines 33–80 and 124–187. Caller sampler, Outside/Unknown, fallback, canonical sites, deterministic seed, ordinary failure values; no map owner. |
| S5 | [Grid geometry](../projects/engine/src/assets/geometry/grid-geometry.cpp), lines 18–49; [anchor geometry](../projects/engine/src/math/anchor.cpp), lines 56–67. Four vertices per independent cell strip; normalized top-left pivot produces local Y-up positions and flipped source UV coordinates. |
| S6 | [Submission declaration](../projects/engine/include/cheryl/assets/submission/draw2d.h), lines 24–57; [implementation](../projects/engine/src/assets/submission/draw2d.cpp), lines 11–85; [packet resolver](../projects/engine/src/core/rendering/draw-packet.cpp), lines 8–33. Default image binding, asset/default material selection, checked `4*cell` range, retained resources/copied parameters, CPU validation. |
| S7 | [Frame/pass writers](../projects/engine/include/cheryl/core/rendering/render-frame.h), lines 16–24, 50–61, 65–117. Insertion assigns authored order; group add validates/reserves before append; frame recycling releases packet handles on the graphics owner. [Packet contract](../projects/engine/include/cheryl/core/rendering/draw-packet.h), lines 13–42, retains values and treats order sensitivity as metadata only. |
| S8 | [OpenGL playback](../projects/modules/graphics/opengl/src/backends/opengl/renderer.cpp), lines 186–198. Iterates passes/packets in order and checks the native domain; [pipeline draw](../projects/modules/graphics/opengl/src/backends/opengl/pipeline.cpp), lines 241–270, draws one range unless its clip is empty. [Pipeline definitions](../projects/engine/include/cheryl/assets/definitions/pipeline.h), lines 40–55; [builtin material construction](../projects/modules/graphics/opengl/src/backends/opengl/builtin-materials.cpp), lines 118–124, preserve depth-disabled ordinary rendering. |
| S9 | [Concrete asset demo](../projects/apps/demo/src/asset-demo.cpp), lines 21–22, 60–100, 137–174. Manifest/images/tileset lookup, required targets, pivot-aware placement, update-owned time, `cell_at -> resolve_tile -> pass.add`. [Demo frame consumer](../projects/apps/demo/src/demo-game.cpp), lines 369–379, supplies pass/context/style. This is compatibility evidence, not personal taste. |
| S10 | [CPU submission cases](../projects/engine/tests/all-tests/src/assets/submission.cpp), lines 18–69 and 226–297, supply synthetic resources and trace selected tile -> animation -> packet retention with zero native calls asserted. [Selection cases](../projects/engine/tests/unit/src/tileset-selection.cpp), lines 60–95 and 121–168, specify timing, non-recursion, bounds and negative time. [Frame cases](../projects/engine/tests/all-tests/src/assets/render-frame.cpp), lines 104–201, specify copied state, order, recycling and invalid group preservation. Cases were read, not run. |
| S11 | [Rectangle overlap](../projects/engine/include/cheryl/math/collision.h), lines 7–38. Existing Y-up positive-area overlap excludes touching edges; it does not compute a cell range or convert map/camera coordinates. |
| S12 | [Game hooks](../projects/engine/include/cheryl/core/game-framework/abstract-game.h), lines 9–34 and 64–102. Upload/binding setup belongs to platform init, updates/preparation share simulation ownership, preparation may be skipped, writers are borrowed, cleanup waits for frames. |

The current [asset guide](../docs/site/assets/asset-values-and-playback.md),
especially its application-map example at lines 162–237, and the
[asset system limits](../docs/site/engine-systems/assets.md) at lines 138–141
corroborate this separation. They describe current intent/example use, not an
already implemented tilemap or user endorsement of the proposed API.

## Historical preference evidence and limits

History was read only from this clone's Git objects, including a bounded read-only
subagent review followed by direct inspection of the cited examples. Reproduce
an archived location with `git show <commit>:<path>`; line spans refer to that
blob, not today's moved files. No network or protected checkout was used.

| ID | Exact provenance and inference |
| --- | --- |
| H1 | Import `eb3f81c219e779b0200c1e82e5bc7239386528e8` (`V3.0a (#12)`, cppcooper, 2026-10-14): `include/abstracts/singleton.h` lines 7–10 explicitly delegates construction to the caller; lines 21–27 and 98–107 provide construct-before-get and retained construction. `include/core/resources.h` lines 12–19 concretely constructs `FileMgr` and retains a reference. This is deliberate v3 setup vocabulary; confidence that it implies a tilemap API taste is low. It gives no reason to make map instances singletons. |
| H2 | Same import: `include/resource-management/factory.h` lines 5–44 defines typed creation/destruction/forwarding. `src/resource-management/resources.cpp` lines 3–16 attempts manifest -> type lookup -> loader -> filename retention. `include/core/resources.h` lines 16–27 leaves policy/revision TODOs; the implementation references unresolved policies. These are tentative mechanisms, excluded as proof of an approved loader, ownership model or disk prerequisite. |
| H3 | Same import: `include/types/tile-set.h` lines 10–31 and `src/asset-types/tile-set.cpp` lines 79–95 mutate `m_FrameIndex` through subscript then draw it. A concrete loading consumer is `src/core/game.cpp` lines 78–97. However `include/core/game.h` lines 3–10 labels the framework v1.0.2/2018, and supporting `include/types/basic-types.h` lines 29–38 records a 2015 update. This is legacy-associated material inside the v3 import, not evidence for a current preference for mutable tilesets, modulo bounds, global maps, or disk loading. |
| H4 | Distinct replayed modern baseline, not automatically v3: `5ddd560c4c4fc298d1d6af92355e333c1b1ebbc8` (`Adds assets`, Josh Cooper, author date 2024-09-25), `include/cheryl/assets/2d/tileset.h` lines 14–38 separates data/Tile/Tileset, while retaining mutating subscripts; `src/assets/2d/tileset.cpp` lines 18–19 constructs a temporary Tile to draw. `d69b0a2377dc9f4b688904ddca5d199c5b49ad11` (`Adds asset loading & managers`, same author/date), `include/cheryl/templates/asset-mgr.h` lines 17–28 returns typed shared handles, and `src/resources/assets/tileset-mgr.cpp` lines 5–13 constructs/caches tilesets. This is limited structural evidence for typed handles/small values; mixed mutable APIs prevent stronger preference claims. |

The annotated tag `3.0a` peels to
`75bfca596c7246539c604d51d2a0d36a2577f40b`. The import aggregates incomplete/WIP
changes rather than preserving per-file original chronology. Commit
`8854ef84f360fafa3572b58ee5440b5aabc0e67c` explicitly retires the archived tree
and restores the baseline before replaying modern work; the two histories must
not be conflated. No inspected archive path supplies an intentional map consumer,
coordinate convention, layered editor, visible-region policy or two-instance
example. H1's explicit intent is more useful than import membership, but remains
weak evidence for tile APIs. Recent source/APIs, even with the established commit
identity, establish compatibility rather than personal preference.

Accordingly, confidence in explicit finite/two-layer/two-instance requirements is
high (the assignment); confidence in source compatibility is high by inspection;
confidence in the proposed storage/caller ergonomics is medium; personal API
preference remains low and is never presented as a user decision.

## Implementation units and observable acceptance

This design assignment ends here. These units describe subsequent work; they do
not activate implementation or resource-heavy checks now.

| Unit | Coherent result and boundary |
| --- | --- |
| U1 — next | CPU map values, checked edits/queries, and visible-range math only; focused tests for TM-001/003/004/006. Proposed owners: `include/cheryl/maps/tilemap.h`, `src/maps/tilemap.cpp`, `tests/unit/src/tilemap.cpp` under `projects/engine/`. No rendering/assets dependency. See [next.md](next.md). |
| U2 | Stable retained bindings and `Submission::resolve_tilemap_layer` in a separate `assets/submission/tilemap2d` header/source; synthetic CPU packet cases for IDs, placement, order, snapshot retention, invalid groups and animation. Exercise the small caller shape before adding other callers. |
| U3 | Dedicated existing-demo sample helper: authored fixture, two instances, two layers, paint/erase/show/pan controls using existing input contexts; actual visual checks once authorized prerequisites are available. Keep optional UI modules unnecessary. |

Acceptance remains entirely pending. The following observations must eventually
be made, not inferred from this record:

| Check | Required observable result |
| --- | --- |
| CPU creation and edits | Construct a 3×2 two-layer map without renderer/game/assets. All 12 cells start empty; set/read/erase affect exactly one cell/layer. Negative/right/bottom/invalid-layer edits reject and preserve values; overflow dimensions reject before allocation. |
| Visibility | TM-004 examples produce exactly the listed ranges, including fractional and negative boundaries, all four exterior edges, full containment, zero area, inverted/nonfinite rejection, and extreme finite off-map rectangles without unsafe casts. |
| Authored layers | An overlapping synthetic ground/detail pair has consecutive `authored_order`, ground first; reversing submission reverses it. A translucent/transparent detail fixture visually shows the ground appropriately with disabled depth. |
| Display after edits | A paint or erase appears in the next prepared frame, including after moving the camera away and back; a retained older packet vector is unchanged. Empty IDs emit no packet, while atlas target zero draws normally. |
| Instance independence | Two displayed maps may share immutable bindings; modifying/hiding/pausing A leaves B's cells, time and output unchanged. Destroy A and resolve B again. Deep-copy edits likewise cannot alias. |
| Coordinate placement | Noncenter source pivots align the four corners to the intended map-cell footprint; first/last rows are not vertically reversed. World-point picking and visible selection agree at exact edges and after a camera pan/positive zoom. |
| Resolution failures | An unbound visible ID, invalid target/resource/material, invalid source frame/pivot, unrepresentable transform or invalid elapsed time produces no result vector and leaves a previously populated pass unchanged for that layer. Setup failures identify the required fixture. |
| Lifetime/CPU boundary | Synthetic resource counters show no native bind/draw/upload during resolution; packets retain resources/copied transforms after maps/bindings are released. Normal runtime cleanup recycles frames before backend shutdown. |
| Independence | Reset/recreate either map entirely in memory; no disk persistence, level transitions, physics, sibling branch or optional GUI is needed. Missing art is recorded as unavailable acceptance, never as a passing skipped sample. |

Existing registration was traced: `projects/engine/tests/CMakeLists.txt` lines
4–13 routes the older submission/frame cases into shared unit sources;
`tests/unit/CMakeLists.txt` lines 1–8 includes them and new unit sources in
`tests-engine`. `cmake/CherylTests.cmake` lines 39–73 supplies its runner and case
registration. Presence in that runner is not a test run. No native build, test,
artwork validation, performance measurement, or visual consumer ran in this phase.
