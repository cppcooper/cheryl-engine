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

`Asset2D` retains image, geometry and an optional selected material generation.
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

`TileAnimationDefinition::cell_at(elapsed)` independently resolves a clip cell from
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
cell does not trigger recursive target substitution. Tileset lookup uses this helper
after choosing the original target.

## Tile selection

The CPU-only `select_tile(rule, sampler, options)` overloads accept Wang, bitmask or
variant definitions and return a `TileSelectionResult`: either a `CellIndex` or a
`TileSelectionFailure`. `MissingRule` means the derived signature/mask has no
candidate; `IncompleteNeighborhood` means a required sample remains unresolved.
Neither failure silently chooses a fallback tile. The caller can skip the tile,
retain an earlier selection or supply its own fallback cell.

The sampler receives a `TerrainSite` with a direction and site kind. Bitmask sites
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
their ID and have independent fallback policies: `Empty`, `Center` or `Unresolved`.
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

`Tileset::cell_at(target, elapsed)` checks the original target against its grid,
resolves only that target's clip at the caller's elapsed milliseconds, and checks the
returned frame cell. Static targets return their original cell. A frame that is
itself an animation target does not trigger another lookup. Negative time throws
`invalid_args`, including for static targets; original or returned cells outside the
grid throw `bad_request`. A selected clip validates its full timeline, but only its
returned cell is checked against the grid. Direct construction still needs valid
cells for later frames; loader construction validates all of them.

`Tileset::select_tile(name, sampler, options, elapsed)` combines rule selection and
one-time animation substitution. Unknown rule names throw `std::out_of_range`;
selector failure values pass through without inspecting a clip. Negative elapsed
time is rejected before sampling even when a rule would fail. Identical stable
samples, seed and elapsed time produce the same result. To retain a base choice
between frames, use the free selector once, then call `cell_at` with new simulation
times. These queries do not advance a cursor or bind resources.

During frame preparation, pass the resolved cell to the existing CPU submission
overload. For an existing Tileset, sampler, options, simulation elapsed milliseconds,
draw style/context and `RenderPassWriter` named `pass`:

```cpp
const auto selection = tileset.select_tile("terrain", sampler, options, simulation_elapsed);
if (const auto* cell = std::get_if<CE::Assets::CellIndex>(&selection))
    pass.add(CE::Assets::resolve_tile(tileset, *cell, style, context));
```

The selection result owns only a cell/failure value. The resulting packet copies
draw state and retains graphics resources; it carries no sampler, live world or
clock. Reusable
[CPU checks](../development/architecture-validation.md#engine-asset-and-text-regressions)
do not establish artwork appearance, missing metadata or a world/map API.

## Fonts and CPU submission

`Font::layout()` returns owned glyph placements and does not retain the input text.
Indices address six-vertex glyph quads; offsets are local Y-up pen positions in the
font's metrics. STBFont decodes UTF-8 and uses baked printable-ASCII glyphs. Space
advances without a glyph, newline resets X and subtracts line height, carriage return
is ignored, and tab advances four spaces. Each unsupported scalar selects one `?`;
malformed input selects one `?` per maximal subpart using the
[encoding contract](text-encoding.md). Combining sequences remain separate scalar
placements because this atlas supplies no shaping or non-ASCII glyphs. Callback
traversal uses the same decoding/spacing rules; exceptions propagate with prior
callback effects intact. Alternate-bank layout is rejected. FFont retains a separate
[legacy metric/bank contract](../resources/legacy-ffont.md).

The separate [Unicode text service](text-layout.md) shapes accented Latin/Cyrillic,
resolves paragraph bidi and optional local-width wrapping, selects whole-grapheme
fallback and prepares immutable glyph pages. Its `RenderedText` overload submits
already uploaded placements and matching page handles; supply `context.image` for
page selection. Legacy Font implementations retain their existing APIs/metrics.

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
