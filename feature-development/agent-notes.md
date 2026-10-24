# Tilemap milestone: Engine terrain owner and retained submission

This branch implements U1's CPU values/ranges, U2's retained binding/packet
adapter, U3's synthetic pass consumer, U4's private fixed demo display, and U5's
checked CPU point picking/pick-paint-erase consumer. U6 adds shared private editor
state and compiled demo event wiring. U7 proves a private cardinal terrain consumer;
U8 promotes that same implementation into Engine; U9 adds bounded atomic batches.
Graphical input/display acceptance stays open.
Executable acceptance and current verification limits are recorded in [next.md](next.md).
The reader summary is [decisions.md](decisions.md); the next handoff is
[next.md](next.md).
Decision IDs TM-001 through TM-011 identify the same choices in both views.

The design inspected source baseline `3542f883391d4225329dab8d5d61a5987e69c8f0`;
U1 starts at design checkpoint `2e74a09ccd56f55d6256699ed69668ba6dbd7c00` on
`feat/tilemap`, with a clean index and tree. Workspace authority remains the
parent `AGENTS.md`, `coordination/autonomy-policy.md`, tilemap feature brief and
the bounded coordinator assignments. U1 was accepted at
`dec1ec55b1c8d55cfed22ffcf2003238a9878a09`; U2 starts there with a clean index/tree.
U2 was accepted at `c77496f0adace3e76c9a59fc573a1a0ef138eb63`; U3 starts there
with a clean index/tree. U3 was accepted at
`f91e181b2eb2870f90baea61161ddd38e1803db6`; U4 starts there with a clean
index/tree. U4 source/build was accepted at
`6a85f160ffa3f3acd56312f707ebbb65bde5e607`; U5 starts there clean. Only this
clone is owned. U5 was accepted at `ff76e5d6381243b05d2b450dd176d5b7a3dbc620`;
U6 starts there clean. It changes private demo state/wiring and explicitly adds
the private app source/include to the existing engine unit case target. Public
Map/picking/resolver/input/camera/frame/pass/backend/startup/persistence APIs and
dependency pins remain unchanged. U6 is accepted at
`fb0feefb62307138db88fa01a5488eb6d8885753`; U7 starts there clean, changing only
private unit consumers and internal records. U7 is accepted at
`e97f6e84bf6a160261a89e22ccecb61eeaa4a5b9`; U8 starts there clean and promotes
only that proven owner plus a normal Engine consumer and focused ownership
verification. U9 starts clean at completed U8
`79c02fbc7f903e7e87e5afc2dbf4b70fa1797032`, after inspecting its ready receipt
and saved 64/64 checks. The publisher owns transport.

## Small U6 event caller and owner boundary

```cpp
#include "tilemap-editor.h" // private app header; also first in the CPU consumer cases

DemoTilemapEditor editor;
// On simulation, in publication order, using only copied tick data:
editor.consume(tick.input.records(), {tick.logical_size, tick.framebuffer_size});
// Later on that same owner, with no concurrent update or frame observation:
editor.write(frame, bindings, image_style);
```

DemoTilemaps owns this state and stable bindings; required Reader/decode/manager
loading remains on platform. Tilemap mode acquires its Events capture lease before
its init_platform early return, samples initial logical/framebuffer sizes there,
then transfers state to serialized simulation update/preparation. Its existing
init_simulation return intentionally installs no ordinary UI hooks or action
bindings. Cleanup resets the lease and helper on platform after simulation joins
and frames finish. No borrowed tick records, camera, native window, provider or
frame/pass view survives a helper call. Ordinary demo modes keep their old paths.

Controls (private engineering choice, not user taste): 1/2 west/east; G/T ground/
detail; 3–6 brush IDs 7–10; left paint/right erase/middle sample, including ID zero;
WASD pan by 32 world units per Press; wheel Y scales zoom by exp2(delta/4) at its
own captured pointer; R resets pan/zoom only. Initial selection is west/ground/7.
Only gameplay-admitted keyboard/mouse records act. Button Release/Repeat, other
devices, missing pointer positions, final Pointer/Axis records and text do not
edit. Input sequence is consumed as supplied, never regrouped or sorted.
Selection affects subsequent records immediately, rather than the whole tick.

TickContext already supplies paired logical_size/framebuffer_size sampled by
GameRuntime::select_window on platform, including sequential and threaded update
paths. GLFW mouse/scroll callbacks capture glfwGetCursorPos into each record.
These are observation-time logical positions but tick-selected sizes, not a
per-event resize history. A resize between observation and consumption may thus
map with the selected tick's sizes; no historical-window reconstruction is claimed.
No public correction is needed. Evidence: input-record.h, tick-context.h,
game-runtime.cpp::select_window/update and native-glfw input-system.cpp::on_mouse_button/on_scroll.
No actual graphical input has been observed.

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

## Small U4 private demo caller

The application-local `--tilemap-display` mode bypasses optional samples, UI,
audio, fonts, shader reload and input controls. The existing startup/runtime still
owns the native context, finite `--max-updates` stop and frame lifecycle:

```cpp
// GameImpl members: optional<DemoTilemaps> tilemaps_; image_shader_; camera_.
// init_platform(), after existing main:images material loading:
tilemaps_.emplace(); // two independent 3x2/two-layer CPU values
tilemaps_->load(asset_root_, engine_.resources()); // required atlas; no optional skip

// prepare_render_frame(), using the existing writer supplied by the runtime:
auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
const CE::Rendering::Submission::Context2D context{
    pass.semantics(), pass.parameters(), pass.constraints()
};
CE::Rendering::DrawStyle2D images; // identity model, unit scale
images.material = image_shader_;
tilemaps_->write(pass, images, context); // west ground/detail, east ground/detail

// deinit_platform(), after the runtime has finished/recycled its frames:
tilemaps_.reset(); // release bindings before the image material/backend
```

The helper calls `resolve_tilemap_layer` and `pass.add` four times directly, as
in U3. It creates no scene, capture service, engine API or runner. TM-009 records
its data, validation, ownership and alternatives; actual build/native/image
evidence belongs to next.md. Fixture names, flag spelling, colors and placement
are local engineering choices under delegated authority, with low personal-taste confidence
and reversal cost confined to private app files.

## Small U5 CPU caller

The public addition is independent of map storage, assets and cameras:

```cpp
#include <cheryl/maps/tilemap.h>

namespace Maps = CE::Tilemaps;
Maps::Map map({3, 2}, 2);
const auto picked = Maps::pick_cell(map.extent(), {0.1, 0.1}, {0.15, 0.05});
if (picked)
    map.set({1}, *picked, {7}); // owned Point{1,0}; TileId{0} erases later
```

`tilemap.pick_caller` executes this shape, checks the other layer and erases the
cell. It retains the coordinate after map destruction. `pick_cell` has no map
or camera owner, allocates nothing on valid input and returns optional<Point>.
invalid_args distinguishes invalid geometry/nonfinite input from ordinary misses.
The naming and optional-plus-exception shape follow current checked query/value
contracts; personal API-taste confidence is low, and renaming is a local source
migration plus relinking users of the new symbol (TM-004).

The synthetic consumer keeps a small inverse transformation in its own file:

```cpp
// camera-plane p is X-right/Y-up; view = (world - pan) * zoom.
// Validate pan/zoom/origin, finite float matrices and a positive viewport first.
const Maps::LocalPoint local{
    pan.x + p.x / zoom - origin.x,
    origin.y - (pan.y + p.y / zoom)
};
const auto picked = Maps::pick_cell(map.extent(), cell_size, local);
if (picked)
    map.set(ground, *picked, {10});
// Prepare another owned Frame with camera-derived local visible bounds;
// submit west ground/detail, east ground/detail through the existing pass.
// Repeat the pick with TileId{0}, then prepare a separate erased Frame.
```

No transformation, camera framework, input wiring or editor service is added to
the engine. See TM-003/004/005/006 for precise supported transforms, geometry and
frame observations. Source presence, executed CPU cases and still-missing visual
acceptance remain separate in next.md.

## Proposed editable caller sequence

The broader editable/animated consumer below remains a proposal. `CE::Tilemaps` CPU
values and `Submission::{TilemapBindings,TilemapView,resolve_tilemap_layer}` exist;
U3 exercises synthetic pass composition; U4 adds a static local-art display path;
this full editor/animation sequence remains unimplemented and unexecuted.
Its historical Puny World example needs artwork absent from the current pin;
it is not a prerequisite for U4 or U6's local-atlas editor. U6 implements the
smaller event/edit/view subset above; this full animation example stays proposed.
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
and resource stability obligations still apply. This is a coordinator-selected
engineering contract under delegated authority, clarified in U3, with high
compatibility confidence; it is not an explicit user API preference. No
metadata-copy layer, hot replacement or ongoing
revalidation is implemented. There is no hot replacement API in this milestone. Missing nonzero
bindings throw `bad_request` with logical ID, layer and cell coordinates; no silent blank
fallback. The U6 private consumer paints only IDs sampled from its maps or its registered brushes. Copies own independent binding
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
finite positive map-local units (64×48 in U4), independent of the source
image's cell dimensions. The map-local footprint is
`[0, columns*width) × [0, rows*height)`; all products used for placement must remain
finite. A view's world origin `O` is the map's top-left, with world Y increasing
up. Cell `(x,y)` spans world X `[Ox+x*w, Ox+(x+1)*w)` and world Y between
`Oy-(y+1)*h` and `Oy-y*h`.

U5's test-local ViewSample describes pan P, finite positive zoom z and a finite
positive viewport W×H in camera-plane X-right/Y-up units. It applies
`view=(world-P)*z`; inverse world position is `P+view_point/z`, then local
position `(world_x-Ox, Oy-world_y)`. Derived local visible bounds are
`{Px-Ox, Oy-(Py+H/z), Px+W/z-Ox, Oy-Py}`. It passes matching orthographic
projection and scale/translation matrices into the existing writer. No rotation,
perspective, top-down platform mouse conversion or live Camera is involved.

Before mutation, the local consumer requires finite points/origins, positive
finite zoom/viewport values that remain nonzero finite floats, finite float
projection/view matrices, finite inverse results and a positive-area transformed
viewport. Extreme translations can collapse those viewport edges despite finite
matrices and are rejected. It does not clip supplied points to the viewport;
the map query decides membership, leaving later real-input UI gating separate.
The converter uses ordinary double arithmetic and render matrices are floats;
no arbitrary-magnitude world ULP/round-trip guarantee is made. The exercised
pan values and z=2 or 0.5 are exactly representable. These are consumer-local
engineering restrictions, not public camera policy or inferred user taste.

U6 implements that transform once in private DemoTilemapEditor for both the app
and new synthetic tests. For logical size Lw×Lh, framebuffer W×H and pointer q,
camera-plane p is `{qx*W/Lw, H-qy*H/Lh}` in the full framebuffer viewport, with
independent horizontal/vertical scale and top-left to Y-up conversion. Then
world=`P+p/z` and local=`{world.x-Ox, Oy-world.y}`. Pointer positions must be
finite and inside the half-open logical window. pick_cell alone decides grid
ownership against computed edges; no epsilon or quotient index is introduced.
Both picking and camera-derived visibility use the same checked state. Render
matrices are finite floats; CPU inversion uses doubles, so GPU rounding at
arbitrary magnitudes/zooms remains a precision limit, not exact visual evidence.

The helper accepts only positive dimensions and positive finite zoom that stays
positive as float, float-representable pan/zoom/translation, finite projection/view
matrices and finite noncollapsed visible bounds for both maps. Invalid view
replacement preserves the previous view. Invalid/minimized size samples suspend
all events and emit no pass until valid sizes return, preserving maps, view,
selection and brush. Wheel candidates must have finite deltas/pointer and a
positive finite resulting zoom; invalid/overflowing/underflowing candidates are
ignored without clamping. This costs a little repeated validation but avoids an
invented global camera domain. Confidence: high engineering, low UI preference;
reversal is confined to private app source/tests, with no public ABI change.

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
positive footprint products and increasing final cell edges first. U5 shares
this checked geometry boundary with picking. Invalid extents, inverted/NaN/infinite
inputs or overflowing/collapsed required footprint products throw `invalid_args`; zero-area input and a
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

U5 implements `pick_cell(Extent, CellSize, LocalPoint) -> optional<Point>`.
It validates the same geometry and both finite coordinates before classifying
an exterior point. A valid exterior point returns nullopt; invalid inputs throw
invalid_args without mutation. Signed zero belongs to cell zero. Local top/left
are inclusive, each internal exact computed edge chooses the following cell,
and final right/bottom edges are exclusive. For an interior point, the shared
binary upper-bound search yields the first edge greater than the point; subtract
one to get its cell. There is no quotient, epsilon or floating-to-integer cast.
The pure query retains nothing and uses O(log columns + log rows) comparisons,
constant storage and no valid-path allocation, even at maximum uint32 extents.
Invalid-argument exception diagnostics can allocate under the existing policy.
Geometry queries do not require that the product be addressable as Map storage.

Both queries require nonzero axes, finite positive sizes, finite positive total
products and `edge(n)>edge(n-1)` at each final edge. On the exercised binary64
platform, n<2^32 is far below the 53-bit significand's integer limit: normal
product rounding error is much smaller than a step. Subnormal sizes are positive
integer multiples of denorm_min; these bounded products also cannot lose a cell
step. Thus finite positive inputs cannot collapse an interior or final grid
edge under this supported geometry. The explicit positive/final-edge guards are
defensive and do not narrow previously valid binary64 geometry. A size already
rounded to zero is rejected as zero, not rescued by an epsilon. No artificial
unreachable collapsed-grid fixture or OOM probe is manufactured; the consumer's
collapsed transformed viewport has a concrete rejection case. Widening extents
past uint32, alternate floating environments or arbitrary-scale render precision
would require revisiting this argument; they are not claimed here.

The independent point oracle enumerates small half-open cell rectangles, testing
left<=x<right and top<=y<bottom, never sharing production search/quotient code.
It probes exact edges, both nextafter neighbors, interiors and positive/negative
exteriors on asymmetric 5×3, 3×2 and 1×4 domains. Sizes include decimal 0.1,
0.3/0.7, thirds/sevenths, minimum subnormals, minimum normals, 1e-100, 1e100 and
large finite footprints near double's maximum. Separate constant-sized checks
query maximum uint32 axes with unit/decimal/subnormal/large sizes without Map
allocation or cell enumeration. Decimal 43*0.1 and 3*0.1 regress the floor error.

Given axis-aligned world bounds `(L,R,B,T)`, the consumer supplies local visible
rectangle `(L-Ox, Oy-T, R-Ox, Oy-B)` per instance. Its inverse picking uses the
same computed grid boundaries after conversion; bare floor(point/size) remains
insufficient. Culling emits partially visible whole cells and does not scissor
pixels. Camera translation and positive zoom remain consumer responsibilities.

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
U5's camera-derived consumer emits 6 packets initially, 7 after painting one
previously empty west ground cell, then 6 after erase. Existing detail at that
same cell follows the new ground packet; all orders are consecutive. The first
camera culls east after exactly its first column; shifted pan/zoom instead
excludes west column zero and includes all east columns. These are consumer
cases, not changes to pass, renderer or backend behavior.

Default 2D pass constraints and ordinary textured materials disable depth;
straight-alpha composition makes later detail draws visible over earlier ground
draws (S7–S9). A custom material must match the pass and cannot rely on a hidden
depth override. Alternatives include implicit layer Z or automatic material
batching. Those introduce competing ordering policies. Confidence: high because
the brief explicitly requires authored order and current playback preserves it;
there is no v3 evidence for a preferred layer API.

U6 keeps the same west ground/detail, east ground/detail submission order as U4.
New editor frames have 15 packets initially, 16 after painting an empty ground
cell, and 15 after erase. All details and sibling payloads remain unchanged
apart from shifted assigned orders; pan to an exact grid boundary culls west
column zero and leaves 12 packets. There is no selected-layer Z or sort policy.

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
U5 additionally picks camera point (80,40) with pan (90,170), zoom 2, giving
world (130,190), local (30,10), cell (1,0). Paint ID 10 on ground, prepare a
newer Frame, pick/erase to zero and prepare a third Frame. Only that west cell
changes; the complete other layer and sibling map remain unchanged. Detail and
sibling packet payloads stay equal apart from assigned authored_order shifts.
Older and painted frames preserve copied matrices, ranges and resources after
erase and after maps/bindings/provider destruction. Misses, nonfinite inputs,
invalid zoom/pan/origin, inverse overflow and a collapsed transformed viewport
preserve all map cells and the retained older frame. Bind/draw/upload counters
stay zero. This is synchronous CPU composition evidence; no publication/concurrency
behavior is inferred. Alternatives: bake frames into map cells, or rebuild a mesh
after each edit. These complicate identity and stale state. Confidence: high for
existing playback compatibility, medium for the intentionally simple recompute
strategy. A cache later needs explicit invalidation and animation handling.

U6's real private editor helper consumes ordered records and writes a new frame
through existing packet/pass APIs; it never calls native resources. Two clicks
at different positions in one tick must edit two distinct cells even if final
Pointer/Axis samples point elsewhere. Selection and brush changes act in order;
Release/Repeat do not paint, erase, sample or pan. Middle sampling may select
zero, so a later left press can intentionally erase. No held-button brush stroke,
input rebinding, undo, scene/editor framework or transaction across a tick is
promised. Tests exercise full map snapshots and old/new prepared frame ownership.
Invalid events preserve the relevant values/view; earlier valid events remain
applied. Confidence is high for the local engineering contract, low as user UI
taste. Changing controls/state is private app/test migration only.

U7 adds a different, single-terrain-edit transaction: resolve all center/cardinal
results before publishing one semantic label and derived visual cells. This
strong preservation rule does not change U6's ordered event semantics or add
whole-tick/frame rollback. U9 extends the owner with an atomic bounded batch
(TM-011); an invalid later request/cap/derivation preserves every terrain value
for that stroke, without rolling back unrelated events or whole frames. See
TM-007 for Engine ownership and TM-011 for budgets and final-semantic derivation.

## TM-007

**Keep semantic terrain separate and derive visual IDs through explicit mapping.**
U7 established the private prototype; U8 promotes that same algorithm into
`CE::Tilemaps::TerrainMap` in `cheryl/maps/terrain-map.h` and Engine's
`src/maps/terrain-map.cpp`. The test-owned producer/header are removed; all eight
pure cases and both retained-frame callers import Engine instead. The public
constructor, destructor, label/tiles queries and edit operation are out of line
and exported through the normal shared library. U9 adds edit_batch under the
same owner and exports, with its distinct transaction/budget contract in TM-011. No alternate archive or direct
implementation source is linked into consumers. This makes the proven boundary
reusable, at the cost of a public STL-bearing ABI and source/relink migration for
later spelling changes. Engineering confidence is high; spelling is a delegated
choice with low user-taste confidence, not a v3 preference inference. No demo
compiled input, terrain controls, World or pin changes; batch behavior is selected
only by the later U9 assignment.

```cpp
#include <cheryl/maps/terrain-map.h> // first include; link Cheryl::Engine
using CE::Tilemaps::TerrainLabel;
using CE::Tilemaps::TerrainMap;

CE::Tilemaps::Map base({3, 2}, 2);
const std::vector<TerrainLabel> labels{{1}, {1}, {0}, {1}, {2}, {0}};
// rule is caller-supplied Tilesets::Rule containing a FourNeighbor BitmaskRule.
// Its cases return selection targets 0..3 in this synthetic consumer.
const std::array target_to_id{CE::Tilemaps::TileId{7}, CE::Tilemaps::TileId{8},
                             CE::Tilemaps::TileId{9}, CE::Tilemaps::TileId{10}};
TerrainMap terrain(base, {0}, labels, rule, target_to_id);
const auto affected = terrain.edit({1, 1}, TerrainLabel{1});
// On the same serialized owner, prepare with existing stable retained bindings:
pass.add(CE::Rendering::Submission::resolve_tilemap_layer(
    terrain.tiles(), {0}, bindings, view, style, context));
```

TerrainLabel is a distinct uint32 value domain; zero means empty, every other
value is opaque semantic identity. Labels are row-major for one selected layer;
columns increase right and rows down. The owner copies the entire base Map,
selected layer index, exact labels, rule metadata and target table. It derives
only that layer; other layers retain their copied authored values. No mutable
Map reference, temporary sampler, mapping span, event span or external rule alias
is retained. Copy/move of the owner is disabled, avoiding implicit compound
assignment invariants; the underlying Map value/copy/move contracts do not change.
`label(Point)` returns a checked owned value; `tiles()` borrows a const live Map.
Serialize construction inputs, edit/read and preparation, without concurrent
frame observation/recycling. Frames retain copied packets/resources, not this owner.

One rule/table applies to all nonempty labels in this first consumer. Neighbors
connect only when their semantic labels equal the nonzero center, irrespective
of their current visual TileIds. Thus different labels can share a sprite, and
one label can use many visual IDs depending on mask. The constructor accepts
only a Rule holding a FourNeighbor BitmaskRule with 1–4 unique cardinal directions in authored
bit_order, validated through existing Selection::select even for all-empty grids.
Case keys outside the declared bit domain reject. N=(0,-1), E=(1,0), S=(0,1),
W=(-1,0); vector position owns the bit, so N/E/S/W gives weights 1/2/4/8, whereas
W/S/N/E gives 4/8/2/1 for N/E/S/W. Compatible subsets remain supported; omitted
directions create no mask bit. Diagonal directions, EightNeighbor and all Wang
rules reject invalid_args before consumer storage copies. No edge/corner labels
or diagonal gating model is fabricated.

Sampler calls occur synchronously within select(BitmaskRule), with Known semantic
labels in bounds and Outside beyond the finite grid. Outside fallback is Empty;
Unknown never occurs here. There is no wrapping/clamping or borrowed unloaded
world. Empty centers bypass selection and directly derive TileId{0}, even if no
case/table entry exists. Nonempty centers must find an exact mask case. Selected
original targets index the copied dense target_to_id table; its length defines
the accepted target domain and a zero entry is unmapped. A used missing case,
out-of-range target (including SIZE_MAX) or zero entry throws bad_request with
cell/label or target diagnostics. Unused missing cases/targets/holes are permitted
until needed; an all-empty grid may have empty cases and table. Any nonzero
logical TileId, including UINT32_MAX, is valid here; retained binding validity
is a separate resolver boundary. This consumer does not select game assets.

Construction checks nonzero base extent, size_t product, vector max_size, exact
label count, selected layer and rule shape before copying consumer storage.
A base is already a checked dense Map, not a request to allocate an arbitrary
domain; a moved-from base rejects. Full derivation prepares an owned layer vector
before assigning internal tiles and returning the object, without mutating inputs.
Copy/allocation failures propagate; no OOM injection is performed. Memory includes
the base copy, semantic layer and initial derived layer plus copied metadata/table.
No new application memory-budget API or file representation is selected.

`edit(Point, TerrainLabel)` validates signed bounds before any +/-1 arithmetic.
Since valid points lie in a uint32 extent, cardinal offsets fit int64. A same-label
edit returns an empty owned vector with no invalidation or resolution. Otherwise
its unique invalidation list is center then in-bounds N/E/S/W (one through five
points, including empty neighbors), never diagonals. Resolution overlays only
the proposed semantic change while reading the old semantic vector. All pending
TileIds and the owned return vector exist before publication. Only then assign
one label and at most five prevalidated fixed-layer cells; these operations need
no allocation. Private extent/layer invariants and serialized ownership prevent
a mid-commit observer. Missing rules/mappings, invalid bounds and allocations
before commit preserve both domains. This is one terrain-edit transaction, not
rollback for a whole input tick, frame, resource resolution or scene.

Full initial derivation is O(cells*layers) for the copied base plus selected-layer
work. Each edit samples at most four semantic neighbors per affected nonempty
cell; rule metadata is cardinal/bounded and no whole-grid copy/recompute occurs.
The returned list reports invalidated cells even if their visual output stays
equal. Tests compare this against independent whole-grid Manhattan-distance
enumeration and independent full semantic-mask derivation over 384 edit steps.
Literal masks cover isolated/asymmetric/corner/edge/interior cases, reordered and
subset metadata, shared visual IDs across labels, different IDs within one label,
empty/outside and 1x1/1xN/Nx1 domains. Failures after earlier valid candidates
preserve complete labels/tiles and prepared frames. Synthetic retained resources
show 9-to-8 packets on a successful edit, unchanged other layer/map payloads,
resource lifetime after owner destruction and zero native bind/draw/upload calls.

Alternatives: deriving connectivity from visual IDs loses semantic information;
copying the full grid for every edit is unnecessary for this cardinal dependency;
mutating each neighbor immediately would expose partial edits on a later refusal.
Per-label palettes, rule replacement, Wang/diagonals, chunking and interactive
terrain controls remain deferred. Existing packet resolution animates original
bound targets once, separately; this helper neither advances clocks nor passes
an already-final animation selection. Existing selector source supplies
compatibility, not v3 preference evidence. See S4/S19 and next.md for actual checks.

U8 also anchors the existing invalid_args and bad_request diagnostic RTTI in
Engine using exported out-of-line virtual destructors. Their explicitly defaulted
copy/move constructors preserve the former implicit behavior and noexcept
contract; categories, field layout and refusal boundaries are unchanged. The
public owner does not invent a new diagnostic family. Consumers must rebuild
and relink with this Engine; this is a source-tree shared GNU/Linux check, not an
installed SDK, Windows, cross-STL or stable binary-compatibility promise.

The separate tests/terrain-consumer executable includes the public header first,
constructs from short-lived inputs, observes literal masks, edits/clears one
label, preserves a second owner and other layers, catches/retains cross-library
errors, and checks a late refusal. It links only Cheryl::Engine and inherits its
C++/include/link usage requirements; it has no unit-private source, app/JSON
policy or macro patches. A separate neutral header probe uses the existing
consumer helper. This narrow check is registered with CHERYL_BUILD_TESTS and
leaves the broad CHERYL_BUILD_CONSUMER_TESTS family OFF. Actual runtime results,
export/import ownership and registration correspondence belong in next.md.

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

**Use a dedicated private mode, adding tested editing in U6.** U4 implements
private `DemoTilemaps` in the existing demo, selected by `--tilemap-display`.
Two independent dense 3×2 maps each have ground/detail layers. Their full local
footprints are `{0,0,192,96}` with 64×48 cells; west/east world top-left origins are
`{48,240}` and `{328,208}`. Base model is identity, style scale one, detail alpha
0.5 and elapsed zero. U4 was fixed; U6 retains this initial content and adds
private selection/edit/pan/zoom state without animation.
Rows are authored top-to-bottom:

| Instance | Ground rows | Detail rows |
| --- | --- | --- |
| west | `7,8,9 / 10,0,7` | `0,10,0 / 9,0,0` |
| east | `9,7,0 / 8,10,9` | `10,0,10 / 0,7,0` |

IDs 7–10 bind to original targets 0–3 (brown/black/grey/red). Empty zero has no
binding; target zero draws. Four explicit submissions emit 5+2+5+3 packets.
Translucent details overlap ground and one east detail covers an empty ground
cell, distinguishing later-layer blending from a fully opaque replacement.
The maps have distinct placement/content; no hidden instance sorting or Z policy
is introduced. Fixed full-map visible rectangles exercise the existing range
helper in U4; U6 derives those bounds from its checked view. Visual edits remain
unobserved pending changed display access.

Platform init uses the authoritative Reader for the committed local
`graphics/definitions/tilesets/tileset.json`, requiring qualified ID
`tileset:tileset`, its exact resolved texture path, parsed 1×4/100×100 grid,
zero origin/spacing, pivot `{0,1}` and no clips. Decode must yield 400×100 RGBA8;
validate all chosen source rectangles, upload those pixels through Images::Manager,
then load/retrieve through Tilesets::Manager and recheck retained metadata/image
dimensions before binding. Bound metadata is never mutated through any alias.
Failures propagate with the required ID/source path, never an optional skip.
No Puny World/dungeon/magecity artwork is required or downloaded.

The existing material loader provides main:images and default depth-disabled
pass contracts. Platform owns loading; simulation edits the private helper and
preparation reads it serially and copies packets. Existing runtime shutdown finishes frames
before deinit_platform resets the helper, then materials/caches/backend follow
their existing cleanup owners. No reusable API changes are needed;
the demo source glob includes the helper; the existing unit target privately
compiles its CPU source with a private app include path, without enabling demo or
native modules. Existing-pinned native dependencies
are prepared with independent local Git storage; no new dependency/pin is added.

An optional sample would hide required failures and clutter the image. A separate
app/scene would duplicate lifecycle/setup. The private mode is a narrow reversible
choice, with high engineering confidence from existing ownership/contracts and
low confidence as personal UI/API taste. Changing fixture constants/selection
only affects these app files and records. Build, context access, actual display
and captured observation are separate evidence in next.md. U6 tests private
editing/selection/view events and compiles demo wiring. Real input, independent
visibility/time controls and graphical editor acceptance remain unverified or
deferred. A successful compile/CPU event stream does not resolve X11 access.

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

## TM-011

**Apply one bounded atomic batch against final semantic labels.** U9 adds public
TerrainEdit, TerrainBatchLimits and TerrainMap::edit_batch in the existing CPU
header/source. Last-write-wins coalesces duplicate points after complete signed
bounds validation; every uint32 TerrainLabel remains valid, including zero empty
and UINT32_MAX. No invalid-label sentinel or application policy is introduced.
Missing masks/targets remain the existing bad_request boundary. Engine exports
the new operation; its aggregates contain only owned scalar values.

```cpp
#include <cheryl/maps/terrain-map.h> // first include; normal Cheryl::Engine linkage
const std::array stroke{
    CE::Tilemaps::TerrainEdit{{0, 0}, {1}},
    CE::Tilemaps::TerrainEdit{{1, 0}, {1}},
    CE::Tilemaps::TerrainEdit{{0, 0}, {2}},
};
auto affected = terrain.edit_batch(
    stroke, {.requests = 3, .affected_cells = 6, .work = 64});
// On this serialized owner, prepare the existing tiles()/bindings/pass caller.
```

All input coordinates and request work are checked before constructing the owned
proposal. Then coalesce final labels and remove centers whose final label equals
the original. Keep first request occurrence of surviving centers, visiting center
then N/E/S/W; append each in-bounds point only once, preserving first occurrence.
No diagonals. Existing edit() keeps its center/N/E/S/W order unchanged. A center
can appear as another center's earlier neighbor; it is still returned only once.
Even a semantic change with equal visual IDs invalidates its full cardinal union.
Samples read final proposed labels, never partially painted/cached visual IDs;
only final masks need resolve. Intermediate labels may need absent masks without
preventing a valid final batch.

Limits are explicit maxima, not reservations. requests counts all provided records,
including duplicates/no-ops/cancellations; exceeding it rejects before scanning.
One work unit admits each request, visits each net changed center and all four
cardinal candidates (even exterior/duplicate), resolves each unique affected cell
(even empty), and invokes each selector sample (even exterior). For a successful
call W = R + 5*C + A + k*F: input records R, net changed centers C, affected union A,
nonempty final affected cells F, authored cardinal bit count k (1..4). A resolution
unit includes selection when nonempty. The sampler debits before every call; a
short budget can refuse after earlier candidates without publishing anything.
Remaining counters only decrement, avoiding overflowing product/sum calculations.
Empty input needs zero units/storage; same-label/cancelling input needs only R
units, returns empty and performs no selection. Zero caps are valid maxima.

Request count bounds coalescing storage; affected_cells bounds unique pending/output
cells, including empty neighbors. Ordered map/set lookups make auxiliary time
logarithmic in bounded request/affected sets. Logical work is not allocator bytes,
comparison instructions or wall time. Scratch is O(R+A), allocated only for actual
requests/union, never from the numeric caps; SIZE_MAX caps with tiny inputs stay
tiny. No full-grid copy/rescan occurs. Caller spans remain stable for the call;
owned proposals, pending visuals and output precede publication. Allocation failure
or any bounds/rule/mapping/cap refusal preserves both complete domains. Commit
assigns only prevalidated fixed storage with no remaining allocation/refusal.
Serialize reads/edits/preparation as before; no retained samplers/spans, thread
service, undo, frame transaction, Wang, storage, chunks or sibling import.

Alternatives: sequential edit() calls can expose an incomplete stroke or reject
an intermediate mask that final semantics do not need. Whole-grid copies would
make tiny brushes scale with map area. Sorting output row-major would break the
natural one-center order; first-occurrence traversal extends it deterministically.
Engineering confidence is high in these boundaries; spelling, last-wins policy
and logical budget granularity are delegated choices, not inferred user taste.
Reversal requires public source/relink migration. Actual accumulated execution
and separate ABI/leak/display limits are in next.md.

## Source evidence at the inspected baseline

These are source observations, not executed acceptance. S1–S12 line spans refer
to `3542f883391d4225329dab8d5d61a5987e69c8f0`; S13 describes U1's current
branch-local files; S14 identifies U2, S15 the U3 consumer, S16 the U4 app and
S17 the U5 CPU picking/editing addition; S18 the U6 private state/input consumer;
S19 the U7 cardinal terrain prototype. Links resolve here.

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
| S16 | U4's private [display helper](../projects/apps/demo/src/tilemap-demo.cpp), [mode option](../projects/apps/demo/src/demo-options.cpp), [main caller](../projects/apps/demo/src/main.cpp) and [platform/preparation/cleanup owner](../projects/apps/demo/src/demo-game.cpp). Reader -> decode -> image upload -> tileset manager -> stable bindings -> four layer vectors -> existing pass; runtime results and ignored local artifacts are in next.md. |
| S17 | U5's [CPU query/header](../projects/engine/include/cheryl/maps/tilemap.h), [shared geometry/search implementation](../projects/engine/src/maps/tilemap.cpp), [five point cases](../projects/engine/tests/unit/src/tilemap.cpp) and [three transformed consumer cases](../projects/engine/tests/unit/src/tilemap-consumer.cpp). Owned optional coordinates, computed-edge ownership, independent rectangle oracle, bounded maximum extents and local pick/paint/erase plus camera-derived pass preparation. |
| S18 | U6 [private helper](../projects/apps/demo/src/tilemap-editor.cpp), [mode wiring](../projects/apps/demo/src/demo-game.cpp), [consumer cases](../projects/engine/tests/unit/src/tilemap-consumer.cpp) and [private build requirements](../projects/engine/tests/unit/CMakeLists.txt). Existing input-record.h/tick-context.h provide captured pointers and copied sizes; game-runtime.cpp::select_window and both update paths publish sizes, native-glfw input-system.cpp::on_mouse_button/on_scroll captures logical pointers. No public producer change. |
| S19 | U7-proven owner promoted by U8: [public header](../projects/engine/include/cheryl/maps/terrain-map.h), [Engine derivation/edit source](../projects/engine/src/maps/terrain-map.cpp), [literal/oracle cases](../projects/engine/tests/unit/src/tilemap-terrain-cases.cpp) and [retained packet consumers](../projects/engine/tests/unit/src/tilemap-consumer.cpp). Reuses [Selection::select implementation](../projects/engine/src/assets/selection/tile-selection.cpp) and [BitmaskRule definitions](../projects/engine/include/cheryl/assets/definitions/tileset.h); selected targets are size_t ordinals, not semantic labels or logical tile IDs. |
| S20 | U9 [batch declaration/contracts](../projects/engine/include/cheryl/maps/terrain-map.h), [shared derivation/batch staging](../projects/engine/src/maps/terrain-map.cpp), [six literal/oracle batch cases](../projects/engine/tests/unit/src/tilemap-terrain-cases.cpp), [retained-frame batch consumers](../projects/engine/tests/unit/src/tilemap-consumer.cpp) and [normal Engine caller](../projects/engine/tests/terrain-consumer/main.cpp). Saved u9-* native logs prove execution/imports; these agent-authored APIs are compatibility evidence, not user taste. |

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

U1/U2/U3, U4's limited source/build checkpoint and U5 are coordinator-accepted.
U6/U7 are accepted; U8 is complete with public shared-Engine evidence. Only
TILEMAP-U9 atomic terrain batches are active, independently of editor controls.
Actual commands/limits are in next.md; real input/display remains unobserved.
X11 access is not retried.

| Unit | Coherent result and boundary |
| --- | --- |
| U1 — implemented source | CPU map values, checked edits/queries, assignment/move invariants and computed-edge visible ranges; 15 focused cases. Owners: `include/cheryl/maps/tilemap.h`, `src/maps/tilemap.cpp`, `tests/unit/src/tilemap.cpp` under `projects/engine/`. No rendering/assets dependency in the CPU API. See [next.md](next.md) for actual verification limits. |
| U2 — implemented source | Retained bindings and `Submission::resolve_tilemap_layer` in separate `assets/submission/tilemap2d` header/source; synthetic CPU packet cases for IDs, metadata, placement, vector order, retention, atomic result failure and animation. No pass insertion or backend work. |
| U3 — implemented consumer | Six synthetic cases compose two instances/layers through existing pass writers; assert authored_order, caller sequence, snapshots, separate failure boundaries and resource release. Header comments clarify the whole binding-lifetime metadata obligation; no production behavior/API change. |
| U4 — implemented display step | Private --tilemap-display helper/mode; required local four-color atlas validation, two independent 3×2/two-layer maps, explicit ground/detail order and platform cleanup. The display-access gap and all 36 CPU regressions are recorded in next.md; no editor/picking controls. |
| U5 — implemented CPU picking/editing | LocalPoint and pick_cell share computed-edge geometry with visibility; five point cases and three synthetic transformed edit cases. All 36 accepted regressions plus eight additions execute together; no demo/input/backend change or display retry. |
| U6 — private editor source | Shared private CPU state for ordered events, independent logical/framebuffer scale conversion, selection/edit/pan/zoom and frame preparation; demo capture/update/cleanup wiring plus seven consumer cases. Execution is recorded below; real input/display remains unverified. |
| U7 — private terrain source | Owned semantic labels and explicit target mapping, cardinal bitmask selection, complete initial derivation and transactional center/cardinal updates; eight pure cases and two retained-frame consumers. Public APIs/demo inputs/pins unchanged. |
| U8 — public Engine owner | Same cardinal algorithm and single-edit result order; exported lifetime/operations and diagnostic RTTI, normal Engine consumer/header probe, no unit-owned producer or demo input changes. |
| U9 — bounded atomic batches | Final semantic overlay, last-wins requests, stable center/cardinal union, explicit request/affected/work limits and complete-result publication; prior single-edit/public ownership contracts retained. |

U1's 15 selected cases passed for CPU creation/edits, rejection, copies/moves and
computed-edge visibility under ASan/UBSan with leak detection disabled. Exact
commands and the LeakSanitizer limitation are in next.md. U2's 15 synthetic packet
cases and all 15 U1 regressions passed together under the same instrumentation.
U3's six consumer cases and all 30 prior regressions passed together under the
same instrumentation. U4 reruns all 36 successfully, but cannot open DISPLAY=:1;
no demo/capture is launched. Exact execution/logs are in next.md. The broader
rendering/editor observations below remain milestone acceptance targets:

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
registration. U6 adds explicit private app compilation/include requirements.
Presence in that runner is not a test run. U6 commands, sanitizer
status and public-header/ABI limits are in [next.md](next.md); prior U1–U5
configurations/results remain in committed record history and preserved local logs.

## U7 execution and next action (historical)

U7 starts clean at accepted fb0feef and keeps all 51 accepted names. The preserved
CPU Debug/shared Engine/tests/WARN/policy-3.5/OFF build compiles the private owner
and eight terrain cases through the existing unit glob; two consumers reuse
retained synthetic fixtures. Public APIs, Engine implementation, editor controls,
CMake selections, dependency pins and demo compiled inputs remain unchanged.
No demo build/run is needed. Header-first private compilation retains
-Wall/-g/-fsanitize=address,undefined. All native commands use the shared wrapper,
normal queue, one job and current adaptive monitor/no-swap/4 GiB admission policy.
There is no resource refusal or permission retry. Full logs are clone-local.

Formatting, initial build and registration exit 0. Registration matches source:
18 consumer (#200–217), 15 submission (#218–232), eight terrain (#233–240),
20 map (#241–260), total 61. Initial full execution exits 8: 60 pass, one failed
new setup assertion, zero skips, 36.52s. All 51 accepted regressions and nine new
cases pass. terrain_rejection expected 11 setup packets but the two full ground
layers plus two west/one east detail correctly yield 15; its intended negative
edit had not run. Only that assertion/comment changes, then rebuild exits 0
and a one-case rerun passes 1/1, zero failed/skipped, 0.42s. Both late missing-mask
and invalid-target refusal paths are now reached and pass. The helper behavior
never changed for this test correction. All 61 unique names have passing
evidence across those runs; no single 61/61 exit-0 rerun is claimed.

The 384-step independent oracle uses full-grid boolean semantic-neighbor
recomputation and whole-grid Manhattan invalidation checks, never the helper or
selector. Literal masks cover authored noncanonical/subset bit orders, boundaries,
empty/minimal domains and semantic/visual identity separation. Failure cases
preserve semantic and visual snapshots after earlier valid candidates, and
retained-frame cases preserve the other layer/map and copied output/resources.
Synthetic native bind/draw/upload counters remain zero. Old frames survive owners;
weak resources expire only after every frame/vector releases them.

No source warning or sanitizer diagnostic occurs. Only command-local
detect_leaks=0 remains permitted for the LSan tracing limitation; ASan/UBSan and
halt_on_error remain enabled, without changing global settings. All exact argv,
immediate exits, failed-setup diagnostic, registration correspondence and counts
are in next.md and ignored u7-* logs/results under build/tilemap-u1-debug.
Both initial and corrected LastTest logs are preserved. Internal links/fences,
matching TM-001–010 IDs, compact word limits, whitespace and staged diff receive
static review before the local checkpoint.

U4's unchanged probe exited 255, `Error: unable to open display :1`; U7 does not
retry it. Actual app decode/upload, graphical input/display, capture, visual
terrain edits, graphical cleanup and leaks remain unverified. No installed SDK,
Windows/other compiler/STL/GLM ABI or Release acceptance is inferred. This private
prototype is not a shipped Engine API. Low-confidence spellings/placement are
reversible in the consumer files; semantic-label separation and selected scope
come from the coordinator assignment, not inferred v3 taste.

The U7 handoff proposed later private editor integration or reusable promotion;
the coordinator now selects promotion in U8. U7's evidence above remains historical,
including its two-invocation correction. Graphics observation still requires
changed access and a bounded owned-window check. No Wang/diagonal model,
persistence, assets/pins, sibling integration or recurring loop is activated.
Publisher owns transport; the finite queue controller selects subsequent units.

## U8 execution and next action (historical)

U8 starts clean at accepted e97f6e8 and promotes the proven implementation into
normal shared Engine. Constructor/destructor/label/tiles/edit are exported out
of line; the private producer is gone. The public header is first in the pure
terrain cases and separate consumer. Existing case bodies/names are unchanged;
only include/using declarations migrate. Diagnostic key destructors anchor
invalid_args/bad_request RTTI in Engine without changing categories/data fields;
defaulted copy/move constructors preserve the old noexcept behavior.

Preserved Debug/shared/WARN/policy-3.5/optional-OFF build targets only tests-engine
and tests-terrain-consumer plus necessary dependencies. No demo inputs or pins
change and no demo rebuild/run is needed. The narrow consumer is enabled only
with shared Engine/tests; broad consumer tests stay OFF. It inherits public
C++23/includes/sanitizers from Cheryl::Engine and compiles the neutral header
probe, with no direct implementation copy, private app/JSON policy or macro
patches. It executes copied values/short-lived inputs, edits/erase/no-op,
independent owners/other layers, retained typed diagnostics and late refusal.

Initial build exits 1 because the new consumer setup called nonexistent
Map::erase. Correction uses set(..., TileId{0}); incremental build exits 0 and
links both runners; the header probe compiles. Shared-only registration regeneration
also exits 0. Initial ownership helper exits 1 because its local nm parser split
C++ demangled names at spaces, while every native nm/readelf subcommand succeeds.
Correcting that parser yields exit 0; no production ownership fix is concealed.
Complete diagnostics, exact commands and immediate exits are preserved locally.
The rebuild reports seven dangling-else warnings only in unchanged unit/shared
test sources; final builds report none in U8 inputs.

One accumulated invocation passes 64/64, zero failed/skipped, 37.21s, exit 0:
18 tilemap_consumer + 15 tilemap_submission + eight tilemap_terrain + 20 tilemap
(61 accepted names) plus three new external consumer modes. Discovery-to-U7 and
execution-to-discovery sets match exactly. This is U8's single green invocation;
U7's two-run result remains distinct above. Existing independent oracle sequences,
refusal/ownership contracts and zero synthetic native bind/draw/upload assertions
all pass. ASan/UBSan/halt_on_error stay enabled without diagnostics. Only invocation-
local detect_leaks=0 applies for the known LSan tracing failure; leaks are unverified.

Actual nm confirms Engine exports all five public operations, imported by both
consumer object and unit executable; compile database/link map show one Engine
terrain producer and no old unit copy. Diagnostic destructors/RTTI are exported
by Engine, with RTTI imports in the consumer object. The normal GNU PIE executable
uses ELF COPY relocations for Engine-provided RTTI/vtables, so final executable
nm V entries do not imply a test source producer. Typed catches/typeid/casts and
retained exception_ptr/message checks execute across this shared boundary.
readelf records libcheryl-engine.so.4; the actual Ninja link line contains only
Engine plus compiler/runtime requirements. Full nm/readelf dumps, ownership
checker, registration correspondence, LastTest copy and u8-results.json accompany
u8-commands.sh under ignored build/tilemap-u1-debug. next.md lists actual argv,
logs, immediate exits and the exact selected cases.

All native work uses the shared wrapper/normal queue, one job, adaptive monitor,
no swap and 4 GiB host admission floor. Waiting sessions are preserved; no
resource refusal, device run, unchanged display retry or limit bypass occurs.
U4's X11 failure remains `Error: unable to open display :1` (exit 255). Actual
app decode/upload, input/display, capture, graphical teardown and leaks remain
unverified. Public concrete STL layout requires compatible compiler/STL and
rebuild/relink; no installed SDK, Windows/other-toolchain, static or Release
acceptance follows. Comments are source-reviewed; full API rendering is untested.

The committed U8 queue receipt names actual nonempty native logs and is ready
only after this implementation/check evidence. It is a worker execution report,
not automatic coordinator review or graphical acceptance. U8 has no unresolved
CPU prerequisite. Next action is coordinator review, then controller selection
of pinned U9 atomic terrain batches after this run drains. No batch API, demo
terrain controls, sibling integration, new assets/pins, transport or recurring
unit is started by this worker. Publisher owns transport; stop at this checkpoint.


## U9 execution and next action

U9 starts clean at completed 79c02fb after reading its public contract, ready
receipt and saved 64/64 checks. It implements only bounded atomic semantic
batches. Shared derivation is reused by constructor/edit/batch; single-edit
result order, owned rule/mapping/labels and previous refusal contracts remain.
The six-operation header-first caller links only normal Cheryl::Engine, with
no macro patches, private implementation copy, new dependency/pin or app input.
TerrainMap fields/layout are unchanged; new aggregates/method require a matching
rebuilt Engine. All public diagnostic RTTI remains anchored by Engine.

Initial format/build and the later duplicate/cancelling request-cap assertion
build exit 0, without warnings/errors. The existing neutral header probe compiles.
Ownership inspection exits 0: constructor/destructor/label/tiles/edit/edit_batch
are Engine exports and consumer/unit imports, with only one Engine source
producer. Normal compile/link requirements and DT_NEEDED libcheryl-engine.so.4
are saved, preserving the previous diagnostic COPY-relocation interpretation.
No static/SDK/Windows/other-toolchain or stable cross-STL ABI proof is inferred.

Registration exits 0 and retains every U8 name: 20 consumer + 15 submission +
14 terrain + 20 map unit cases, plus four public caller modes, total 73. One
accumulated invocation passes 73/73, zero failed/skipped, 42.53s, exit 0. Exact
name sets match U8/discovery/execution. Six new pure cases test last-wins overlap,
first-occurrence order, final-state sparse masks, all uint32 labels, exact/one-short
request/affected/work caps (including subset and exterior samples), all-empty/
no-op/cancelling semantics, late mask/target/mapping/bounds refusals and 320
independent full-grid oracle batches. No incremental helper/selector is used
by the reference. Two retained-frame cases show 9-to-8 packets, input destruction,
full sibling/layer isolation, old/new resource retention/release and seven
initially valid failure fixtures. Native bind/draw/upload counters remain zero.
The public caller adds a batch mode; all previous caller/header/export checks pass.

After execution, final format validation exits 1 for an overlong newly added
trailing comment; shortening it exposes adjacent-comment alignment (exit 1).
A formatter-only correction then final dry-run exit 0 preserve every C++/assertion
token and source line count, as independently checked in u9-comment-correction.log.
Only comment wording/alignment changes after the 73-case invocation; no behavior
change or additional runtime pass is claimed. Full diagnostics are preserved.

All native commands use the shared wrapper, one job and preserved CPU Debug/shared/
WARN/policy-3.5/optional-OFF output/configuration. Normal queue sessions are kept;
no resource admission refusal, bypass or unchanged retry occurs. ASan/UBSan and
halt_on_error remain enabled with no diagnostics; invocation-local detect_leaks=0
is solely for the known LSan tracing limitation, leaving leaks unverified. Exact
commands/exits, selected-name correspondence, ownership dumps/link line/results
and full LastTest output live in ignored build/tilemap-u1-debug/u9-*; next.md lists
the actual procedure and logs. U8/U7 evidence remains historical above, distinct
from this single green invocation. No demo input changes or demo rebuild is needed.

The U9 committed receipt names actual nonempty native logs and reports ready
only after implementation/verification. It is execution evidence, not automatic
coordinator acceptance. There is no unresolved CPU/build/link prerequisite.
Graphical input/display, app decode/upload, capture, graphical teardown and leaks
remain unverified. U4's X11 :1 exit255/error is unchanged and not retried; full
API reference rendering, installed SDK/static/Release/other-platform ABI checks
remain outside acceptance. Next is coordinator review and finite-controller
selection of pinned U10 private demo terrain mode after this run drains. No U10
implementation, renderer/editor wiring, undo, chunking, storage, sibling
integration, transport or recurring work starts here. Publisher owns transport.
