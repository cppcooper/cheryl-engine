# Tilemap milestone: CPU values, packets and pass consumer

This branch implements U1's CPU values/ranges, U2's retained binding/packet
adapter, and U3's synthetic pass consumer. The first graphical consumer remains
a proposal.
Executable acceptance and current verification limits are recorded in [next.md](next.md).
The reader summary is [decisions.md](decisions.md); the next handoff is
[next.md](next.md).
Decision IDs TM-001 through TM-010 identify the same choices in both views.

The design inspected source baseline `3542f883391d4225329dab8d5d61a5987e69c8f0`;
U1 starts at design checkpoint `2e74a09ccd56f55d6256699ed69668ba6dbd7c00` on
`feat/tilemap`, with a clean index and tree. Workspace authority remains the
parent `AGENTS.md`, `coordination/autonomy-policy.md`, tilemap feature brief and
the bounded coordinator assignments. U1 was accepted at
`dec1ec55b1c8d55cfed22ffcf2003238a9878a09`; U2 starts there with a clean index/tree.
U2 was accepted at `c77496f0adace3e76c9a59fc573a1a0ef138eb63`; U3 starts there
with a clean index/tree. Only this clone is owned. U3 adds consumer cases and a
binding lifetime comment, with no production Map, resolver, frame/pass or backend
behavior/API changes. Demo controls, persistence, resizing and chunking remain
unimplemented.

## CPU caller implemented in U1

The new header is included first, with no asset, renderer or runtime includes:

```cpp
#include <cheryl/maps/tilemap.h>

CE::Tilemaps::Map map({3, 2}, 2);
map.set({1}, {2, 1}, {42});
const auto tile = map.get({1}, {2, 1});
const auto visible = CE::Tilemaps::visible_cells(
    map.extent(), {0.1, 0.1}, {0.0, 0.0, 0.2, 0.1});
```

`TileId{0}` erases a cell; every other uint32 value is valid without a palette.
The `tilemap.caller` case exercises this shape. Exact new naming remains a
reversible engineering choice with low personal-preference confidence.

## Small U2 caller

The selected shape, now implemented in the separate submission adapter, uses the
existing retained `Tilesets::Asset`, `DrawStyle2D` and `Context2D` owners:

```cpp
#include <assets/submission/tilemap2d.h>

namespace Submit = CE::Rendering::Submission;
// retained_tileset is a stable shared_ptr<const CE::Assets::Tilesets::Asset>.
const CE::Rendering::DrawStyle2D style{}; // source material, identity model, scale 1
const Submit::Context2D context{};        // existing automatic image binding
const Submit::TilemapView view{
    .origin = {100, 200}, .cell_size = {20, 12},
    .visible = {0, 0, 60, 24}, .elapsed = std::chrono::milliseconds{0}
};
Submit::TilemapBindings bindings;
bindings.add({7}, retained_tileset, 0); // nonempty map identity -> drawable atlas zero
CE::Tilemaps::Map map({3, 2}, 2);
map.set({0}, {1, 0}, {7});
auto packets = Submit::resolve_tilemap_layer(map, {0}, bindings, view, style, context);
```

The `tilemap_submission.caller` case exercises this sequence with synthetic
resources. One packet is returned; the five empty cells emit none. This is CPU
resolution only, with no pass insertion. New naming follows current submission
vocabulary, not an asserted user preference. H1's explicit caller setup intent
is weak supporting evidence; H3's legacy mutable selection is not adopted.

## Small U3 pass caller

With two independent maps, their views and a stable binding table prepared as
above, the synthetic consumer composes existing APIs directly:

```cpp
#include <assets/submission/tilemap2d.h>
#include <core/rendering/render-frame.h>

namespace Maps = CE::Tilemaps;
namespace Render = CE::Rendering;
namespace Submit = CE::Rendering::Submission;
constexpr Maps::LayerIndex ground{0}, detail{1};
Render::Frame frame;
{
    Render::FrameWriter writer(frame);
    auto pass = writer.begin_pass(projection, camera_view, constraints, parameters);
    const Submit::Context2D context{
        pass.semantics(), pass.parameters(), pass.constraints()
    };
    pass.add(Submit::resolve_tilemap_layer(west, ground, bindings, west_view, style, context));
    pass.add(Submit::resolve_tilemap_layer(west, detail, bindings, west_view, style, context));
    pass.add(Submit::resolve_tilemap_layer(east, ground, bindings, east_view, style, context));
    pass.add(Submit::resolve_tilemap_layer(east, detail, bindings, east_view, style, context));
}
// Observe frame.passes() only while its frame is alive and excluded from writing/recycling.
```

`tilemap_consumer.caller` supplies synthetic retained geometry/image/materials,
copies pass semantics/parameters/constraints into Context2D, and adds 3+2+2+1
packets. The table shares handles; bound source metadata must stay unchanged
through every alias for every retaining binding's lifetime (TM-002). The fixture
constructs an actually const Asset and follows that obligation. No scene wrapper,
separate runner, asset file, runtime startup or rendering is introduced. Executed
counts/results and lifetime limits belong to [next.md](next.md).

## Proposed caller sequence

The broader graphical consumer below remains a proposal. `CE::Tilemaps` CPU
values and `Submission::{TilemapBindings,TilemapView,resolve_tilemap_layer}` exist;
U3 exercises synthetic pass composition; this full runtime/artwork sequence
remains unimplemented and unexecuted.
`FrameWriter`, `Context2D`, and asset handles follow the current engine. Exact new naming has low
preference confidence and should stay local to one consumer until exercised.

```cpp
namespace Maps = CE::Tilemaps;                    // implemented CPU domain
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
between whole-value assignments. Construction initializes every cell to empty,
requires nonzero axes/layers, and checks every count multiplication and `vector::max_size`
before allocating. Invalid dimensions/storage counts throw `invalid_args`;
allocation failure propagates. Copy construction gives independent cells; copy
assignment copies before changing any members, preserving the destination if
allocation fails. Assignment may replace dimensions/layers without an in-place
resize API. No mutable cell references or renderer pointers escape.

Move construction/assignment are noexcept, transfer the value, and leave the
source at `Extent{0,0}` and zero layers. Both `get` and `set` then reject with
`bad_request` before indexing storage. Copying/assigning that empty value is safe;
assigning a newly constructed map restores ordinary use. Self-copy and self-move
preserve the value. The sole zero-dimension state is obtained through moves or
whole-value copies/assignments of moved-from values, never ordinary construction.

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

**Separate logical tile IDs, atlas cells, and retained bindings.** The implemented
strong `TileId` wraps `uint32_t`; zero denotes empty and U2 skips it.
Atlas cell zero remains an ordinary drawable cell. Nonzero IDs are application
identities within a palette domain, not global asset IDs, terrain labels, vertex
offsets, or animation frames. `Map::get` returns a `TileId` by value. The CPU map
accepts every representable ID; it cannot validate a renderer's palette.

Setup-owned `Submission::TilemapBindings` now maps an ID to a retained
`shared_ptr<const Tilesets::Asset>` plus an original `Grids::CellIndex`. `add`
rejects zero IDs, duplicate bindings, null assets/images and out-of-grid targets
with `invalid_args`, preserving existing bindings. It validates nonzero grid/frame
axes, checked cell/vertex counts, finite normalized pivots and the complete source
grid inside its image. Axis comparisons precede multiplication to reject enormous
source origins/spacing/counts safely. Every clip needs an in-grid original target
and all in-grid frames; authoritative `Clip::cell_at(0ms)` validates nonempty,
positive, nonoverflowing timelines at setup. This validation does not advance time
or animate a final selection. Unrelated named views/autotile rules are not used or
validated. Geometry/material/range/parameter compatibility remains with existing
`resolve_tile` for each emitted packet.

The table retains handles, not a deep copy or a mechanism that freezes an Asset.
Bound source metadata must remain unchanged through **every alias** for the
lifetime of **any binding** retaining it, including copies of the table. A
`shared_ptr<const Asset>` restricts access through that handle; it does not freeze
an object reachable through another alias. Synchronizing a mutation between
resolution calls does not satisfy this bind-once metadata obligation. Setup
validation and the asset's target lookup index rely on it. Per-call table, asset
and resource stability obligations still apply. This is the explicit U3 caller
contract, with high confidence; no metadata-copy layer, hot replacement or ongoing
revalidation is implemented. There is no hot replacement API in this milestone. Missing nonzero
bindings throw `bad_request` with logical ID, layer and cell coordinates; no silent blank
fallback. The future consumer paints only registered IDs. Copies own independent binding
tables while sharing source handles under that metadata contract; no lookup reference or mutation handle
escapes. A private friend resolver keeps the public table surface at `add`.
Reversal cost is local to this new submission header/source and its caller cases;
changing exposed STL layout still needs consumer recompilation. Requiring an image
at binding setup is narrower than generic manually assembled `Content2D`; manual
sampler insertion remains supported when that source image is retained.

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
`Extent` is `{columns, rows}` with `uint32_t` axes and the layer count is uint32.
`LayerIndex::value` and `Point::{column,row}` are signed 64-bit values, allowing
negative external inputs to reject before any unsigned cast. Legal cells satisfy
`0 <= column < columns`, `0 <= row < rows`, and `0 <= layer < layer_count`.
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
(S5). Reject origins, cell dimensions, whole-map far edges, scales or resulting
transforms that cannot remain finite floats, and nonfinite pass matrices.
Positive source-to-cell scales must stay positive after float conversion, including subnormal cases. Per-cell source
corner arithmetic also checks finite results after the float model transform.
This follows metadata-compatible Y-up source geometry; Buffer2D cannot prove that
an arbitrary producer uploaded vertices matching its declared frame/pivot.
Ordinary float rounding may collapse adjacent positions at extreme magnitudes;
there is no arbitrary-scale pixel precision guarantee. The first adapter requires
identity base model and `style.scale == 1`;
placement owns translation/size and rejects other transforms rather than letting
culling disagree. It preserves alpha, material override, custom parameters, and
clip inputs. Arbitrary map rotation, oversized cell overhang, and isometric maps
are outside this milestone. Confidence: medium, a reversible map convention,
not an inferred user preference. Y-up map rows were viable; Y-down rows make
row-major authoring and future neighbor offsets direct.

## TM-004

**Select positive-area visible intersections using half-open bounds.** The implemented
pure CPU helper `visible_cells(extent, cell_size, LocalRect)` returns a copied
cell range `[first_column,end_column) × [first_row,end_row)`. `LocalRect` is
continuous `{left,top,right,bottom}` in map-local Y-down units. Validate nonzero
extent axes, finite edges, ordered axes, finite positive cell size and finite
footprint products first. Invalid extents, inverted/NaN/infinite inputs or
overflowing footprint products throw `invalid_args`; zero-area input and a
rectangle merely touching or outside the map return an empty range.

Intersect with the map footprint using comparisons before searching its edges.
Define each grid edge by the actual double multiplication `edge(i)=double(i)*size`,
including the final footprint edge. For a positive clipped intersection, binary
search finds the first edge strictly greater than its minimum (begin is that
edge's index minus one) and the first edge greater than or equal to its maximum
(exclusive end). All searches stay in bounded integer indices; there are no
floating-to-integer casts and no epsilon. Work is O(log columns + log rows),
with no allocation. Input multiplication/comparison uses ordinary double precision;
this does not promise arbitrary-scale rendering precision.

This corrects the original design's bare floor/ceil division: with size 0.1,
`floor((43*size)/size)` can be 42 and `ceil((3*size)/size)` can be 4. Comparing
computed grid edges preserves exact touching-edge exclusion and distinguishes
both adjacent `nextafter` values. The independent test oracle enumerates complete
cell rectangles and checks positive-area intersections; it shares no search or
division formula with production code. Sizes include decimal, fractional,
subnormal and very large finite values.

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
Future picking must use that inverse conversion and the same computed grid
boundaries as visible_cells: edge i is double(i)*size. Bare floor(point/size)
is insufficient at decimal boundaries; any candidate index must be corrected by
comparisons against those edges, with no arbitrary epsilon. Exact right/bottom
map edges are outside. No picking/editor API is implemented in U2.
Camera translation/positive zoom are consumer responsibilities. Selection is
culling, not a scissor: a partially visible cell is emitted whole and the existing viewport/optional clip cuts pixels.

Alternatives are scanning every cell against `Math::overlaps`, an inclusive
maximum index, floor/ceil candidates corrected against grid edges, or having the
map inspect a live camera. Binary edge searches make the boundary predicates
direct with a bounded logarithmic cost. The existing positive-area
overlap helper (S11) is useful as an independent test oracle, but a computed range
avoids O(total cells) traversal and camera coupling. Confidence: high engineering
confidence; coordinate preference remains medium. U1 implements this algorithm;
written tests and actual execution are distinguished in [next.md](next.md).

Reversal costs remain bounded to U1's new header/source and its focused cases:
the edge-search implementation can change while preserving the computed-edge
contract and independent oracle. Changing public integer widths, value layout
or the vector-backed map representation requires consumer recompilation; no
save format or downstream renderer depends on those layouts yet. Confidence in
the engineering boundary is high, while personal API-preference confidence
remains low. No stable cross-toolchain binary ABI is promised by these values.

## TM-005

**Let authored call order determine layer order.** The first consumer requests
two layers and names index 0 ground and index 1 detail. Rendering one layer
traverses the visible range by increasing row, then column. The caller explicitly
submits ground before detail; it may insert actors, reorder layers, or hide one.
There is no sorting by texture, logical ID, Z value, or unordered container order.
Two instances' relative order is likewise the caller's sequence. Layer count is
fixed between whole-value assignments; names, visibility and editor selection belong to the consumer.
U2 returns only one vector and leaves every packet's default `authored_order`
untouched (zero). The existing pass writer, not this adapter, assigns insertion
order later. U3's synthetic caller adds four vectors from two maps/two layers
through that existing pass writer. It asserts contiguous orders 0–7 across groups
of 3, 2, 2 and 1 packets; reversing the caller's group sequence retains each
vector's row/column order and changes the composed order. A distinct retained
image/material pair moves from position 0 to position 5 in the reordered case,
so grouping by resource cannot silently satisfy the expected sequence. Models
retain Z=0.
These are consumer cases, not changes to pass, renderer or backend behavior.

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

U2 implements CPU packet/time resolution. Each visible nonempty binding
supplies its original target to
`tileset.cell_at(target, view.elapsed)` before packet resolution. Each instance's
nonnegative elapsed milliseconds are advanced by update, even when the runtime
skips preparation. No tilemap-owned thread, clock, or animation cursor is needed.
The shared tileset is never mutated. Editing map A or changing its clock cannot
change map B (S3, S12). U2 exposes elapsed as a copied input, not a clock
owner. `resolve_tile(asset, final_cell, ...)` receives the already-final index
and performs no second animation lookup. Edit/erase is observed on the next
resolution while earlier vectors preserve retained values.

U3 prepares an older Frame, ends its writer scope, edits one west-map ID and
changes only that instance's origin, then prepares a separate newer Frame. The
old copied matrices, selected ranges, resources and authored order must remain
unchanged. The east map's entire cell snapshot and all three of its packet
payloads/order remain unchanged. Frame/pass observations occur only after writer
scopes end, and helpers do not retain borrowed frame spans across recycling.
This is synchronous CPU composition evidence; no publication/concurrency behavior
is inferred. Alternatives: bake frames into map cells, or rebuild a mesh
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
The implemented adapter is a free CPU function in `Rendering::Submission`, separate
from CPU map storage. It borrows stable map/bindings/view/style/context for one
call, computes the visible range, resolves target animation and pivot placement,
then calls existing `resolve_tile(asset, final_cell, style, context)` (S6). It
returns an owned ordered vector of packets; there are no native binds, draws,
uploads, retained map references, or live camera references.

Validate the layer, nonnegative time, identity base model, unit style scale,
finite pass projection/view matrices, valid clip and visible-region/placement
inputs even when no cells emit packets. Only visible nonzero IDs are looked up;
offscreen misses and incompatible offscreen resource ranges are ignored. Source
metadata was validated before insertion; existing material/geometry/range/parameter
validation remains authoritative for each visible cell. Any exception destroys
the function-local vector, returning no result and leaving map, bindings, view,
style and context unchanged. Tests fail after a proven earlier valid packet and
preserve a preexisting caller vector. There is no output-parameter partial result,
pass insertion, native call, or whole-frame transaction in U2. A later consumer
can use `pass.add(vector)` under the existing separate group contract (S7).

U3 now exercises that separate contract. One case successfully inserts two prior
packets, proves the west resolver setup is valid, then changes only a later
visible map cell to unbound ID 999. Its negative assertion invokes only the
resolver, while a geometry-query counter proves an earlier valid cell reached
packet validation. No failed vector is submitted; prior payloads/order survive.
Another case resolves a valid two-packet group, validates both packets against
the actual pass constraints, then changes only the later packet's vertex_count
from 4 to 0 before group add. Rejection must append neither packet and preserve
all prior draws. These cases isolate failure boundaries instead of conflating
resolution with insertion. They promise neither frame rollback nor native work.

Bindings retain assets; packets independently retain geometry, material/pipeline
and images, with copied matrices. They do not keep the map, binding table or asset
metadata alive. Native resource-domain validity still depends on the original
live backend. Existing runtime graphics-owner recycling/cleanup remains required,
even though resolution is CPU-only (S7, S8, S12).

U3's retention case destroys maps, bindings, provider and all other fixture owners
while a prepared Frame is the sole remaining resource owner. The Asset weak
handle expires, while geometry/image/material/pipeline remain retained. Only then
does the case copy a packet vector from the frame. Recycling the Frame releases
its share; the copied vector still owns those resources.
Clearing the vector then expires all four resource weak handles. All borrowed
frame spans/writers are out of scope before recycle. Synthetic geometry/image
bind/draw counters and provider upload-entry counters stay zero by contract;
stand-ins are directly constructed and upload capability is never given to the
consumer. This bounds the evidence to the local fake resources; no backend
resource domain, graphics-owner teardown or visual outcome is established.

For V visible cells and N nonempty cells on a layer, traversal visits O(V) cells
with expected constant-time binding lookup and O(N) output storage. Per-cell costs
also include existing timeline scans and packet/material validation; this is not
a constant-time animation claim. Setup revalidates all clips on each add rather
than caching asset-validation state. Current playback performs one pipeline
invocation per packet (up to N native draws; an empty clip can suppress drawing).
No batching or performance target is claimed. An all-filled 8×6 two-layer map
emits 96 packets; two such maps emit 192. Temporary vectors and repeated packet
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
submodule was uninitialized during design; the coordinator later initialized its
unchanged pin for U1's required embedded font. Tile artwork and these targets
were not independently validated by U1. Validate required cells/clips and report an unavailable
fixture explicitly; an optional-demo skip does not count as acceptance.

Unit acceptance uses synthetic retained geometry/image/material fixtures from
S10 and needs no installed image files or native backend. Later visual acceptance
does need a feature-local asset checkout and compatible renderer. Neither is a
reason to alter dependency pins or retrieve tile artwork during CPU U2.
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

These are source observations, not executed acceptance. S1–S12 line spans refer
to `3542f883391d4225329dab8d5d61a5987e69c8f0`; S13 describes U1's current
branch-local files; S14 identifies U2 and S15 the U3 consumer. Links resolve here.

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
| S13 | U1's current [CPU header](../projects/engine/include/cheryl/maps/tilemap.h), [implementation](../projects/engine/src/maps/tilemap.cpp) and [focused cases](../projects/engine/tests/unit/src/tilemap.cpp). These own map values, assignment/move invariants and computed-edge visibility; current command outcomes are in next.md. |
| S14 | U2's [submission header](../projects/engine/include/cheryl/assets/submission/tilemap2d.h), [implementation](../projects/engine/src/assets/submission/tilemap2d.cpp) and [synthetic cases](../projects/engine/tests/unit/src/tilemap-submission.cpp). Retained ID bindings, checked source metadata, pivot transforms, once-only animation and local ordered vectors; exact executed selections are in next.md. |
| S15 | U3's [synthetic pass consumer](../projects/engine/tests/unit/src/tilemap-consumer.cpp) composes unchanged Map, binding/packet and FrameWriter/PassWriter owners. Six cases cover caller order, independent frame snapshots, isolated resolver/group failures and weak lifetimes. The submission header clarifies all-alias metadata stability over binding lifetime; execution results are in next.md. |

The current [asset guide](../docs/site/assets/asset-values-and-playback.md),
especially its application-map example at lines 162–237, and the
[asset system limits](../docs/site/engine-systems/assets.md) at lines 138–141
corroborate this separation. They describe current intent/example use, not an
already implemented graphical consumer or user endorsement of the new API.

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

U1/U2 are coordinator-accepted. TILEMAP-U3 activates only the synthetic pass
consumer and bind-once lifetime contract clarification. Actual command outcomes
and limitations are in next.md. Later graphical work needs another assignment.

| Unit | Coherent result and boundary |
| --- | --- |
| U1 — implemented source | CPU map values, checked edits/queries, assignment/move invariants and computed-edge visible ranges; 15 focused cases. Owners: `include/cheryl/maps/tilemap.h`, `src/maps/tilemap.cpp`, `tests/unit/src/tilemap.cpp` under `projects/engine/`. No rendering/assets dependency in the CPU API. See [next.md](next.md) for actual verification limits. |
| U2 — implemented source | Retained bindings and `Submission::resolve_tilemap_layer` in separate `assets/submission/tilemap2d` header/source; synthetic CPU packet cases for IDs, metadata, placement, vector order, retention, atomic result failure and animation. No pass insertion or backend work. |
| U3 — implemented consumer | Six synthetic cases compose two instances/layers through existing pass writers; assert authored_order, caller sequence, snapshots, separate failure boundaries and resource release. Header comments clarify the whole binding-lifetime metadata obligation; no production behavior/API change. |
| U4 — proposed display step | Minimal existing-demo display fixture for two in-memory maps/two layers using suitable existing local art. Check actual layered display and placement before choosing editor/picking controls. Requires a later coordinator assignment. |

U1's 15 selected cases passed for CPU creation/edits, rejection, copies/moves and
computed-edge visibility under ASan/UBSan with leak detection disabled. Exact
commands and the LeakSanitizer limitation are in next.md. U2's 15 synthetic packet
cases and all 15 U1 regressions passed together under the same instrumentation.
U3's six consumer cases and all 30 prior regressions passed together under the
same instrumentation. Exact execution/logs are recorded in next.md. The broader rendering/editor observations below remain milestone
acceptance targets, with graphical consumer acceptance still pending:

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
registration. Presence in that runner is not a test run. U1/U2/U3 exact commands,
sanitizer status and public-header/ABI limits are recorded in [next.md](next.md).

## U3 execution and next action

U3's final run passed six consumer cases plus all 30 U1/U2 regressions: 36/36,
no skips, 28.26 seconds, Debug ASan/UBSan. Build, discovery and execution exited 0.
The checks establish caller-controlled order, old/new frame independence, separate
resolver/group failure preservation and weak resource release. Leak checking,
real graphics-domain/visual behavior, installed SDK/Windows and other ABI/Release
contexts remain unverified. The next proposed action is U4's minimal two-map,
two-layer display fixture, after suitable local-art validation and a later
coordinator assignment; no next unit starts here.

The final source uses an actually const bound Asset and keeps all borrowed frame
views outside writing/recycling phases. That honors the metadata caller contract;
it does not enforce every external alias or establish concurrent publication.
The header clarification changes no callable/layout or implementation. Detailed
logs, intermediate passing fixture refinements and policy boundaries are in
[next.md](next.md). Exact executed commands, all from this clone:

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
