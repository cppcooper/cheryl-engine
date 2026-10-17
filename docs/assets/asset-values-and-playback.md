# Asset values and playback

Asset definitions own their strings, paths, grids and clip/rule containers. They are
CPU values with no graphics-thread affinity. Treat a published definition as
immutable when sharing it. Parsing validates document-local references;
[Loader preparation](asset-loading.md) additionally checks cross-document identities
and decoded image bounds. Direct aggregate construction does not perform those
checks automatically.
Parsed texture paths already include the enclosing `graphics/` directory and
lexical normalization; consumers do not prepend the document directory. `textures()` returns
owned deduplicated entry paths in sprite-then-tileset first-reference order.

## API domains

Asset families use contextual names: qualify `Asset`, `Data`, `Definition` and
`Animation` with the family when combining them in one consumer.

| Namespace | Roles |
| --- | --- |
| `CE::Assets::Sprites` | `Definition` and `Clip` own metadata; `Data` supplies construction inputs; `Asset` retains resources; `Animation` owns a playback cursor; `Manager` caches sprites. |
| `CE::Assets::Tilesets` | `Definition` and `Clip` own metadata; `Asset`, `Tile` and `Animation` retain resources; `Selection` owns CPU rule sampling; `Manager` caches tilesets. |
| `CE::Assets::Grids` | `Definition`, `View` and `CellIndex` describe addressing; `GeometryData` and `make_geometry()` provide CPU upload storage. |
| `CE::Assets::Animation` | `TimedFrame` is shared clip metadata; `Cursor` supplies ordinal wrap/clamp playback. |
| `CE::Assets::Graphics` | `Asset` retains a whole-image quad and its image. |

Pixel coordinates, vertex shapes and the uploaded `Buffer2D` contract belong to
`CE::Rendering::Geometry`. Image/program/material interfaces belong to
`CE::Rendering::Resources`; parameters, sampling and CPU packet resolution have
their own `Rendering` namespaces. `CE::Assets::Content2D` composes those retained
resources for the asset families. The [rendering guide](../rendering/pipelines-and-materials.md)
describes their backend and submission boundaries.

## Coordinates and grid geometry

`CE::Rendering::Geometry::PixelPoint`, `CE::Rendering::Geometry::PixelSize` and `CE::Rendering::Geometry::PixelRect` use image pixels. Rectangles have a top-left
origin; their coordinates are widened to accommodate grid arithmetic. A
`CE::Assets::Grids::Definition` addresses row-major cells. `cell_count()` rejects multiplication
overflow, and `cell_index()`/`cell_rect()` reject out-of-range selections. Direct
callers must provide dimensions whose counts and coordinate arithmetic fit their
integer types; the addressing helpers do not validate a grid against an image or
validate every aggregate field. Empty rows/columns produce zero cells; occupied
bounds on an empty axis equal that axis's origin.

`CE::Assets::Grids::make_geometry()` requires nonzero texture dimensions, a fitting occupied
rectangle and an addressable vertex count. It returns retained temporary CPU
storage, with four vertices per cell in independent triangle strips. Keep that
storage through upload; the provider copies it before returning. A complete strip
for each cell is drawn separately to avoid joining adjacent cells. Positions are
local Y-up pixel units about the normalized top-left pivot; UVs are normalized
texture coordinates. Pivot formulas and named views are defined by the
[manifest format](asset-manifests.md#pivots).

## Resources and caches

`CE::Assets::Content2D` retains image, geometry and an optional selected material generation.
Submission uses an explicit draw-style material first, then the asset selection;
it rejects when neither supplies a usable material. Tile and animation handles
retain the same selection. Direct construction permits null
handles and does not prove matching layout, grid bounds or backend domains. Loader
construction supplies validated definitions and uploaded resources; CPU submission
validates its selected geometry/material/range. Read-only resource metadata can be
used during frame preparation. Binding, drawing, linking and uploading obey the
selected backend's owner/current-context rules. Logical handles can outlive cache
or backend teardown, but cannot perform native operations through a closed domain.
See [resource ownership and retirement](../resources/resource-residency.md).

`CE::Assets::Graphics::Asset::from_image()` rejects a null or zero-sized image, then uploads a six-vertex
whole-image quad on the provider owner. The caller must use a compatible image and
provider domain. `load()` first decodes/uploads the image. Failure returns no graphic asset;
these helpers do not publish a cache entry.

Sprite and tileset managers use `namespace:name` IDs, require referenced images to
be loaded, and retain existing IDs. `CE::Assets::Images::Manager` uses lexically normalized paths;
an exact lookup wins, and a bare filename is accepted only if unique. Ambiguous
filenames throw, absent entries return null. Normalization does not resolve symlinks
or fold case. `contains()` uses the cache's exact key, without the filename fallback.
Batch loading can publish earlier entries before a later failure. Serialize loading,
replacement and teardown on the active provider's loading owner. Cache reads copy
strong handles under a lock; those handles survive clear/replacement independently.

## Sprite playback

`CE::Assets::Sprites::Asset` retains one immutable definition snapshot and indexes clips by name/facing.
Construction checks a nonempty grid and nonempty clips with positive millisecond
durations and in-range cells, rejecting duplicate clip keys. It does not revalidate
all views, orientations, image bounds or resource compatibility from a direct
`CE::Assets::Sprites::Data` value. Definition/view references borrow the sprite's lifetime.
Missing views/orientations throw `std::out_of_range`.

Each `animation()` result is a `CE::Assets::Sprites::Animation` retaining its clip
through the shared definition allocation,
so it survives sprite destruction without retaining the sprite's GPU handles. It
has an independent mutable cursor. A faceless lookup prefers an exact faceless clip;
otherwise it accepts a single matching facing-specific clip. Missing or ambiguous
clip requests throw. `has_animation()` applies the same availability rule.

Advance playback on its simulation owner with finite, nonnegative seconds. Invalid
elapsed time leaves the cursor unchanged. `set_frame()` wraps looping clips and
clamps nonlooping clips, resetting time within the selected frame. At an exact frame
duration boundary, `advance()` enters the next frame; nonlooping playback stays at
its last frame. `frame_duration()` reports the current frame in milliseconds.
Copies share immutable clip data but copy cursor/time state. Publish `cell()` to
submission rather than allowing playback and rendering to race on a cursor.

## Tile playback and rules

`CE::Assets::Tilesets::Asset` owns its definition and rejects duplicate animated targets on construction;
direct construction does not validate every frame or autotile rule. Definition,
view and autotile references borrow that tileset's lifetime. Named lookup failure
throws `std::out_of_range`. `tile(cell)` checks grid bounds and returns a handle
retaining the shared resources. `animation_for(target)` returns an independent
playback value or no value; it does not require the target to be a valid grid cell.

`CE::Assets::Tilesets::Animation` copies its clip and retains image/geometry. Its `CE::Assets::Animation::Cursor` index wraps
or clamps according to `loop`; an empty sequence is rejected. The index selects a
clip frame, whose `cell` may be unrelated to its ordinal index. The caller schedules
index changes using the positive per-frame millisecond durations. There is no
elapsed-time advance operation. A directly constructed `CE::Assets::Animation::Cursor` requires its initial
index to be below its nonzero limit; later `set_frame()` normalizes the index.

`CE::Assets::Tilesets::Clip::cell_at(elapsed)` independently resolves a clip cell from
nonnegative elapsed milliseconds. The caller chooses the simulation clock and phase
origin: a shared elapsed value synchronizes tiles, while per-tile values provide
independent phases. Lookup has no cursor and does not mutate the definition or
resources. Each frame owns a half-open interval; at an exact duration boundary,
lookup enters the next frame. Looping clips wrap by total duration and nonlooping
clips hold the last cell after their timeline ends.

Lookup rejects negative time, an empty clip, nonpositive frame durations and total
duration overflow. It validates the complete timeline even when the selected frame
is earlier. Direct definitions still need caller-provided grid/cell validation.
After choosing a base tile, resolve that target's animation once; an animated frame's
cell does not trigger recursive target substitution. Tileset asset lookup uses this helper
after choosing the original target.

## Tile selection

### Sample an application map

This CPU example maps a small game-owned terrain grid to a four-neighbor rule.
Rows increase southward and columns eastward; rendering placement is a separate
game choice. Terrain `0` is empty and `1` is the terrain being connected. Include
`<cheryl/assets/selection/tile-selection.h>`, `<array>`, `<cstdint>`,
`<optional>` and `<variant>`:

```cpp
namespace Tiles = CE::Assets::Tilesets;
namespace Pick = CE::Assets::Tilesets::Selection;
using Cell = CE::Assets::Grids::CellIndex;

const std::array<std::array<std::uint32_t, 3>, 3> map{{
    {{0, 1, 0}},
    {{1, 1, 0}},
    {{0, 0, 0}}
}};
Tiles::BitmaskRule rule;
rule.type = Tiles::BitmaskType::FourNeighbor;
rule.bit_order = {Tiles::Direction::North, Tiles::Direction::East,
                  Tiles::Direction::South, Tiles::Direction::West};
rule.cases = {{0, 0}, {9, 1}, {15, 2}};

const int column = 1;
const int row = 1;
const Pick::Sampler sampler = [&](Pick::Site site) -> Pick::Sample {
    int x = column;
    int y = row;
    switch (site.direction) {
        case Tiles::Direction::North: --y; break;
        case Tiles::Direction::East: ++x; break;
        case Tiles::Direction::South: ++y; break;
        case Tiles::Direction::West: --x; break;
        default: return {Pick::SampleState::Unknown, 0};
    }
    if (x < 0 || x >= 3 || y < 0 || y >= 3)
        return {Pick::SampleState::Outside, 0};
    return {Pick::SampleState::Known, map[y][x]};
};
Pick::Options options;
options.terrain = map[row][column];
options.outside = Pick::Fallback::Empty;
const auto result = Pick::select(rule, sampler, options);
std::optional<Cell> visible_cell;
if (const auto* cell = std::get_if<Cell>(&result))
    visible_cell = *cell;
```

For the center tile, North and West connect: bits zero and three produce mask
`9`, selecting cell `1`. This example deliberately declares only three masks.
Selecting the top-middle tile produces mask `4`, absent from `cases`, and returns
`MissingRule`; `visible_cell` stays empty and the game skips that tile. Add the
remaining authored cases for a complete terrain set. If a required neighbor is
temporarily unavailable, return `Unknown` rather than `Outside`; the default
unknown policy produces `IncompleteNeighborhood` instead of inventing empty land.

Keep this rule and map on their simulation owner. Save the selected cell in the
game's visible state; its sampler borrows the map only during selection. A matching
loaded tileset must have the selected cell ranges and artwork. For that retained
`tileset`, a nonnegative simulation time in milliseconds named `elapsed`, and the
`pass`, `style` and `context` from ordinary frame submission:

```cpp
if (visible_cell) {
    const auto cell = tileset.cell_at(*visible_cell, elapsed);
    pass.add(CE::Rendering::Submission::resolve_tile(tileset, cell, style, context));
}
```

`cell_at()` substitutes the selected target's animation once. Advance `elapsed`
during update, so skipped frame preparation does not pause tile animation. The
packet retains resources and copied draw values; it carries no map or sampler.
The [Rendering system](../engine-systems/rendering.md#integration) supplies the
pass/submission setup, and [Threads and Data Flow](../architecture/threads-data-flow.md)
explains publication to rendering. The contracts below cover Wang sites,
deterministic variants and direct-construction limits.

### Selection contracts

The CPU-only `CE::Assets::Tilesets::Selection::select(rule, sampler, options)` overloads accept Wang, bitmask or
variant definitions and return a `CE::Assets::Tilesets::Selection::Result`: either a `CE::Assets::Grids::CellIndex` or a
`CE::Assets::Tilesets::Selection::Failure`. `MissingRule` means the derived signature/mask has no
candidate; `IncompleteNeighborhood` means a required sample remains unresolved.
Neither failure silently chooses a fallback tile. The caller can skip the tile,
retain an earlier selection or supply its own fallback cell.

The sampler receives a `CE::Assets::Tilesets::Selection::Site` with a direction and site kind. Bitmask sites
are neighboring cells. Wang sites are the selected tile's shared edges or corner
vertices, with nonzero IDs from that rule's terrain catalogue. The consumer owns
coordinates and the conversion from its world representation to these labels;
adjacent tiles must sample the same label at a shared site. Cell-centered terrain
alone does not define how a mixed Wang edge/vertex is labeled. This interface adds
no world storage or map coordinate convention.

Required sites are sampled synchronously once each in canonical north-to-northwest
order, regardless of declared bit order. An unresolved sample still allows the
remaining required sites to be sampled. Keep the rule, options and sampled world
stable for the call; sampler exceptions propagate and no callback/world reference is
retained. Unused directions are not sampled. Read-only queries can run concurrently
when the supplied samplers and world support that access.

`Known` samples carry an ID; zero means empty. `Outside` and `Unknown` samples ignore
their ID and have independent `Selection::Fallback` policies: `Empty`, `Center` or `Unresolved`.
Defaults treat outside-world sites as empty and unknown sites as unresolved. A known
Wang label absent from its terrain catalogue uses the unknown policy. A Wang center
fallback requires `options.terrain` to be declared or zero. Bitmasks have no terrain
catalogue: different known IDs are disconnected rather than unknown.

Wang selection fills its edge or corner slots directly and zeroes the unused slots.
Bitmasks connect equal nonzero IDs to `options.terrain`, and `bit_order[i]` owns bit
`1 << i`. Four-neighbor rules declare one to four unique cardinal directions;
eight-neighbor rules declare one to eight unique directions. Diagonals default to
`RequireCardinals`, which needs both adjacent cardinal cells to connect, including
cardinal directions absent from `bit_order`. `Independent` uses just the declared
diagonal's sample. Any unresolved required sample prevents selection even when
another sample would already disconnect that diagonal.

Wang variants use the caller's explicit 64-bit seed and candidate indices in
definition order. A fixed unsigned seed mixer produces a 53-bit fraction; cumulative
positive weights choose the cell, independently of unordered-map iteration or other
queries. Normalize by the maximum weight before accumulation to avoid overflowing a
sum of finite weights. Double arithmetic and finite draw precision apply: sufficiently
small relative weights may have no representable draw. Terrain `probability` metadata
does not choose the world's terrain or modify these variant weights. The caller
chooses a stable seed per location; animation time does not reseed a variant.

An empty sampler, invalid policies, malformed direction/slot order, or invalid
matched Wang candidate indices/signatures/weights throws `invalid_args`. Candidate
indices must be strictly increasing, and matched weights must be finite and positive.
Unmatched rules and other direct aggregate fields are not fully revalidated. The
free selector returns a base cell without grid checks or animation substitution.

`CE::Assets::Tilesets::Asset::cell_at(target, elapsed)` checks the original target against its grid,
resolves only that target's clip at the caller's elapsed milliseconds, and checks the
returned frame cell. Static targets return their original cell. A frame that is
itself an animation target does not trigger another lookup. Negative time throws
`invalid_args`, including for static targets; original or returned cells outside the
grid throw `bad_request`. A selected clip validates its full timeline, but only its
returned cell is checked against the grid. Direct construction still needs valid
cells for later frames; loader construction validates all of them.

`CE::Assets::Tilesets::Asset::select_tile(name, sampler, options, elapsed)` combines rule selection and
one-time animation substitution. Unknown rule names throw `std::out_of_range`;
selector failure values pass through without inspecting a clip. Negative elapsed
time is rejected before sampling even when a rule would fail. Identical stable
samples, seed and elapsed time produce the same result. To retain a base choice
between frames, use the free selector once, then call `cell_at` with new simulation
times. These queries do not advance a cursor or bind resources.

During frame preparation, pass the resolved cell to the existing CPU submission
overload. For an existing tileset asset, sampler, options, simulation elapsed milliseconds,
draw style/context and `CE::Rendering::PassWriter` named `pass`:

```cpp
const auto selection = tileset.select_tile("terrain", sampler, options, simulation_elapsed);
if (const auto* cell = std::get_if<CE::Assets::Grids::CellIndex>(&selection))
    pass.add(CE::Rendering::Submission::resolve_tile(tileset, *cell, style, context));
```

The selection result owns only a cell/failure value. The resulting packet copies
draw state and retains graphics resources; it carries no sampler, live world or
clock. Reusable
[CPU checks](../development/architecture-validation.md#engine-asset-and-text-regressions)
do not establish artwork appearance, missing metadata or a world/map API.

## Fonts and CPU submission

`CE::Text::Legacy::Font::layout()` returns owned glyph placements and does not retain the input text.
Indices address six-vertex glyph quads; offsets are local Y-up pen positions in the
font's metrics. AsciiFont decodes UTF-8 and uses baked printable-ASCII glyphs. Space
advances without a glyph, newline resets X and subtracts line height, carriage return
is ignored, and tab advances four spaces. Each unsupported scalar selects one `?`;
malformed input selects one `?` per maximal subpart using the
[encoding contract](text-encoding.md). Combining sequences remain separate scalar
placements because this atlas supplies no shaping or non-ASCII glyphs. Callback
traversal uses the same decoding/spacing rules; exceptions propagate with prior
callback effects intact. Alternate-bank layout is rejected. BitmapFont retains a separate
[legacy metric/bank contract](../resources/legacy-ffont.md).

The separate [Unicode text service](text-layout.md) shapes accented Latin/Cyrillic,
resolves paragraph bidi and optional local-width wrapping, selects whole-grapheme
fallback and prepares immutable glyph pages. Its `CE::Text::Rendering::Generation` overload submits
already uploaded placements and matching page handles; supply `context.image` for
page selection. Legacy Font implementations retain their existing APIs/metrics.

`CE::Rendering::Submission::resolve_sprite`, `CE::Rendering::Submission::resolve_tile`, `CE::Rendering::Submission::resolve_graphic` and `CE::Rendering::Submission::resolve_text` perform CPU
submission without binding resources or mutating playback. Keep their inputs stable
for the call. They copy style/pass values and retain geometry, material and any
resolved image bindings. An `CE::Rendering::Submission::ImageParameter2D` inserts the asset image as a draw-layer
sampler using a public contract key and zero-based unit. An empty key, null image or
duplicate draw-layer key throws; omitting it leaves image selection to the other
parameter layers.

Sprite/tileset cell overloads check logical grid bounds; selected tile and
tile-animation values instead rely on their resource range and clip indexing.
Text submission applies scaled local glyph offsets through the caller's model and
returns owned packets; empty/whitespace-only text can return no packets. A failed
resolution publishes nothing to a frame. Returned packets retain everything needed
for playback after the asset/font/text is released. See
[render submission](../rendering/pipelines-and-materials.md#cpu-submission) for packet
validation and frame-writer ownership.
