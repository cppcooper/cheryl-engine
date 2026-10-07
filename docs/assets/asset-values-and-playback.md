# Asset values and playback

Asset definitions own their strings, paths, grids and clip/rule containers. They are
CPU values with no graphics-thread affinity. Treat a published definition as
immutable when sharing it. Parsing validates document-local references;
[Loader preparation](asset-loading.md) additionally checks cross-document identities
and decoded image bounds. Direct aggregate construction does not perform those
checks automatically.
Parsed texture paths already include the source document's parent directory and
lexical normalization; consumers do not prepend it again. `textures()` returns
owned deduplicated entry paths in sprite-then-tileset first-reference order.

## Coordinates and grid geometry

`PixelPoint`, `PixelSize` and `PixelRect` use image pixels. Rectangles have a top-left
origin; their coordinates are widened to accommodate grid arithmetic. A
`GridDefinition` addresses row-major cells. `cell_count()` rejects multiplication
overflow, and `cell_index()`/`cell_rect()` reject out-of-range selections. Direct
callers must provide dimensions whose counts and coordinate arithmetic fit their
integer types; the addressing helpers do not validate a grid against an image or
validate every aggregate field. Empty rows/columns produce zero cells; occupied
bounds on an empty axis equal that axis's origin.

`make_grid_geometry()` requires nonzero texture dimensions, a fitting occupied
rectangle and an addressable vertex count. It returns retained temporary CPU
storage, with four vertices per cell in independent triangle strips. Keep that
storage through upload; the provider copies it before returning. A complete strip
for each cell is drawn separately to avoid joining adjacent cells. Positions are
local Y-up pixel units about the normalized top-left pivot; UVs are normalized
texture coordinates. Pivot formulas and named views are defined by the
[manifest format](asset-manifests.md#pivots).

## Resources and caches

`Asset2D` retains image and geometry handles. Direct construction permits null
handles and does not prove matching layout, grid bounds or backend domains. Loader
construction supplies validated definitions and uploaded resources; CPU submission
validates its selected geometry/material/range. Read-only resource metadata can be
used during frame preparation. Binding, drawing, linking and uploading obey the
selected backend's owner/current-context rules. Logical handles can outlive cache
or backend teardown, but cannot perform native operations through a closed domain.
See the [consumer resource contract](../resources/consumer-resource-contract.md).

`Graphic::from_image()` rejects a null or zero-sized image, then uploads a six-vertex
whole-image quad on the provider owner. The caller must use a compatible image and
provider domain. `load()` first decodes/uploads the image. Failure returns no Graphic;
these helpers do not publish a cache entry.

Sprite and tileset managers use `namespace:name` IDs, require referenced images to
be loaded, and retain existing IDs. TextureMgr uses lexically normalized paths;
an exact lookup wins, and a bare filename is accepted only if unique. Ambiguous
filenames throw, absent entries return null. Normalization does not resolve symlinks
or fold case. `contains()` uses the cache's exact key, without the filename fallback.
Batch loading can publish earlier entries before a later failure. Serialize loading,
replacement and teardown on the active provider's loading owner. Cache reads copy
strong handles under a lock; those handles survive clear/replacement independently.

## Sprite playback

`Sprite` retains one immutable definition snapshot and indexes clips by name/facing.
Construction checks a nonempty grid and nonempty clips with positive millisecond
durations and in-range cells, rejecting duplicate clip keys. It does not revalidate
all views, orientations, image bounds or resource compatibility from a direct
`SpriteData` value. Definition/view references are borrowed from the Sprite.
Missing views/orientations throw `std::out_of_range`.

Each `animation()` result retains its clip through the shared definition allocation,
so it survives Sprite destruction without retaining the Sprite's GPU handles. It
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

`Tileset` owns its definition and rejects duplicate animated targets on construction;
direct construction does not validate every frame or autotile rule. Definition,
view and autotile references are borrowed from that Tileset. Named lookup failure
throws `std::out_of_range`. `tile(cell)` checks grid bounds and returns a handle
retaining the shared resources. `animation_for(target)` returns an independent
playback value or no value; it does not require the target to be a valid grid cell.

`TileAnimation` copies its clip and retains image/geometry. Its `Frame` index wraps
or clamps according to `loop`; an empty sequence is rejected. The index selects a
clip frame, whose `cell` may be unrelated to its ordinal index. The caller schedules
index changes using the positive per-frame millisecond durations. There is no
elapsed-time advance operation. A directly constructed `Frame` requires its initial
index to be below its nonzero limit; later `set_frame()` normalizes the index.

Wang signatures, weights and bitmask cases are metadata. These APIs do not sample
neighbors, choose variants or automatically replace a selected animated target.
That selection layer remains in the [roadmap](../planning/develop-review-and-development-plan.md#u10--deterministic-tile-selection).

## Fonts and CPU submission

`Font::layout()` returns owned glyph placements and does not retain the input text.
Indices address six-vertex glyph quads; offsets are local Y-up pen positions in the
font's metrics. STBFont uses baked pixel metrics and printable ASCII bytes. Space
advances without a glyph, newline resets X and subtracts line height, carriage return
is ignored, and tab advances four spaces. Other bytes use `?` individually, including
each byte of UTF-8. Alternate-bank layout is rejected. FFont has a separate
[legacy metric/bank contract](../resources/legacy-ffont.md).

`resolve_sprite`, `resolve_tile`, `resolve_graphic` and `resolve_text` perform CPU
submission without binding resources or mutating playback. Keep their inputs stable
for the call. They copy style/pass values and retain geometry, material and any
resolved image bindings. An `ImageParameter2D` inserts the asset image as a draw-layer
sampler using a public contract key and zero-based unit. An empty key, null image or
duplicate draw-layer key throws; omitting it leaves image selection to the other
parameter layers.

Sprite/tileset cell overloads check logical grid bounds; selected Tile and
TileAnimation values instead rely on their resource range and clip indexing.
Text submission applies scaled local glyph offsets through the caller's model and
returns owned packets; empty/whitespace-only text can return no packets. A failed
resolution publishes nothing to a frame. Returned packets retain everything needed
for playback after the asset/font/text is released. See
[render submission](../rendering/pipelines-and-materials.md#cpu-submission) for packet
validation and frame-writer ownership.
