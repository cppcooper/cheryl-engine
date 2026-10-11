# Graphics manifests 2.0

Graphics indexes and definition documents use version `2.0`. The authoritative
schemas are [graphics indexes](../../assets/schemas/graphics-manifest-index-2.0.schema.json),
[sprites/tilesets](../../assets/schemas/graphics/sprite-tileset-manifest-2.0.schema.json)
and [shaders/materials](../../assets/schemas/graphics/shader-manifest-2.0.schema.json).
Earlier versions are rejected rather than implicitly upgraded. `$schema` is an
editor hint; the runtime neither fetches it nor discovers schemas as assets.

## Index selection and paths

Only the exact basename `graphics-manifests.json` selects definition documents.
An index contains `version: "2.0"` and a `manifests` array of JSON paths. Full loading
uses every discovered index; focused loading supplies explicit registered indexes.
Repeated document references are parsed once, and indexes cannot reference indexes.
Unlisted JSON remains registered for manual use and is not opened automatically.

Indexes and definitions live beneath a directory named `graphics`. Index references,
texture paths and shader source paths all resolve from that enclosing directory,
regardless of document nesting. Paths use forward slashes and exact, case-sensitive
registered names; absolute paths, parent traversal, backslashes and colons reject.
`$schema` links still follow normal editor-relative resolution.

The checked-in layout separates `graphics/definitions/`, `graphics/textures/`,
`graphics/shaders/` and `graphics/ui/`. The main
[graphics index](../../assets/graphics/graphics-manifests.json) selects sprite/tileset
definitions; the [shader index](../../assets/graphics/shaders/graphics-manifests.json)
selects only the main demo recipes.

The [asset package catalog](catalog.md) lists download sources and image placement
for the packages referenced by the checked-in manifests.

## Identity and inheritance

- A globally unique asset ID is `namespace:name`, combining the namespace and sprite or tileset map key. The loader checks duplicate IDs across manifests before upload.
- An entry's `texture` overrides the manifest-level `texture`. One of those fields is required by the schema.
- An entry's `pivot` overrides `defaults.sprite.pivot` or `defaults.tileset.pivot` according to the containing map.
- Sprite/tileset documents declare `asset_class: "sprite-tileset"`. An entry's
  optional `shader` overrides the document-level selection. It is a qualified
  material ID, independent of the texture path; omitted selections use a supplied
  draw-style material. An explicit draw-style material overrides an asset selection.
- Shader and material are one asset class, declared as `asset_class: "shader"`.
  Their program and material names share qualified identity validation and cannot
  collide with another selected program/material identity.

## Grid and cell addressing

`grid.origin` is the top-left pixel of the first cell. `grid.frame` is one cell's pixel size, and `grid.spacing` is the number of pixels between adjacent cells. All current grids use `row-major` order:

```text
index = row * columns + column
```

Both cell-reference forms are equivalent:

```json
{ "index": 31 }
{ "row": 1, "column": 4 }
```

The engine must reject references outside the grid. The occupied texture bounds are:

```text
right  = origin.x + columns * frame.width  + (columns - 1) * spacing.x
bottom = origin.y + rows    * frame.height + (rows - 1)    * spacing.y
```

Named `views` are rectangles in grid coordinates. Their row and column are relative to the containing grid, not the texture.

## Pivots

Pivots are normalized within a frame and measured from its top-left corner. `{ "x": 0.5, "y": 0.5 }` is frame center; `{ "x": 0.5, "y": 1.0 }` is bottom center. MiniWorld actors default to bottom center, projectile/weapon sprites override that default to center, and tilesets default to center.

For the engine's Y-up quad coordinates, an arbitrary normalized pivot produces these frame vertices:

```text
left   = -pivot.x * width
right  = (1 - pivot.x) * width
bottom = (pivot.y - 1) * height
top    = pivot.y * height
```

The runtime retains numeric pivots for arbitrary normalized positions. `AnchorType` also covers all nine standard positions, including bottom center, so older named-anchor call sites remain available without rounding manifest values.

## Animations

An animation profile describes a regular facing-by-action sprite sheet. For a selected facing and clip:

```text
row = profile.facings[facing] + clip.row_offset
```

Each value in `clip.columns` then identifies a frame on that row. `frame_duration_ms` applies to every frame and `loop` controls wraparound. The `swordsman` profile is attached to the template plus the cyan, lime, purple, and red Swordsman sheets.

Per-sprite `animations` support arbitrary cells and per-frame durations. Tileset
animations additionally identify a `target` cell: simulation-owned elapsed time
selects its frame before submission. If an autotile selects an animated target,
autotile selection happens first and animation substitution happens second.

## Autotiles

Wang signatures always use this explicit order:

```text
north, north_east, east, south_east,
south, south_west, west, north_west
```

Terrain ID `0` means no terrain. `wang-corner` sets use the corner slots and `wang-edge` sets use the edge slots. Multiple cells may share a signature; they are visual variants, selected uniformly unless a `weight` is present.

The schema also supports `four-neighbor` and `eight-neighbor` bitmask autotiles.
`bit_order[i]` owns bit `1 << i`, and the decimal mask string selects a cell from
`cases`. Four-neighbor rules declare one to four unique cardinal directions;
eight-neighbor rules declare one to eight unique directions. Runtime connectivity
and boundary policies are defined by the
[tile selector](asset-values-and-playback.md#tile-selection).

The Puny World manifest uses the author-supplied `punyworld-overworld-tiles.tsx`
and matching PNG tile IDs. Keep artwork and metadata revisions together when
changing that manifest; a different sheet layout cannot preserve those IDs.

## Runtime implementation

The manifest parser, typed asset dispatch, pivot/grid construction, animation
expansion, autotile lookup data, CPU-only rule selector and Tileset animated-target
substitution are implemented. See
[asset-loading.md](asset-loading.md) for the runtime entry point, validation/load
order, retrieval APIs and per-entity sprite playback. Sampling, deterministic
selection and simulation-time substitution follow the
[tile selection contract](asset-values-and-playback.md#tile-selection). Reusable checks are in the
[validation guide](../development/architecture-validation.md#engine-asset-and-text-regressions);
source rule acceptance does not supply missing artwork metadata.

## Shader/material definitions

A shader document contains named `programs` and `materials`. Each program owns
`stages.vertex` and `stages.fragment` source references. Each material names its
qualified `program`, `vertex_layout`, `topology`, typed `parameters` and backend
`bindings`; optional fields are `state`, literal `defaults` and FX `sampling`.
See [main.json](../../assets/graphics/shaders/main.json) for the complete text/image
example: both materials share one program but use different draw topologies.

Definitions contain no texture paths or default images. Textures contain no shader
references. Sprites and tilesets associate those independent resources; submission
supplies images and units to named sampler parameters. Ordinary materials omit
sampling and inherit the engine's policy. An FX `sampling` map can override filtering,
mipmaps, wrapping and anisotropy for a sampler key without selecting an image.
An explicit sampler in the submitted image binding takes precedence.

Parameters support `float`, `int`, `uint`, `bool`, `vec2`, `vec3`, `vec4`, `mat4` and
`sampler2d`; matrix literals contain sixteen column-major numbers. Projection,
view, model, alpha and scale are engine semantics. Defaults are CPU literals and
cannot hold image/backend handles. Arrays, uniform blocks and additional stages
are unsupported. Unknown fields, wrong types/kinds, duplicate identities and
unresolved references reject before publication. Backend payloads remain opaque
to common loading; OpenGL interprets `bindings.opengl.attributes` and
`bindings.opengl.parameters` and validates active shader reflection.

The [loading guide](asset-loading.md) defines preparation, publication and explicit
replacement. [Pipelines and materials](../rendering/pipelines-and-materials.md)
defines manual builders, sampling and retained generation behavior. Only the main
text/image materials are indexed; the other shaders remain manual, unverified
examples. TGUI and RmlUi still construct their materials explicitly.

## Recovered college definitions

[Invaders](../../assets/graphics/definitions/misc/invaders.json),
[HyperMaze](../../assets/graphics/definitions/misc/hypermaze.json),
[Rover](../../assets/graphics/definitions/misc/rover.json) and
[Tileset](../../assets/graphics/definitions/tilesets/tileset.json) preserve the legacy
source ordering and pixel rectangles. Invaders has eleven sections containing
nineteen frames; Rover uses four separate entries because its frame sizes differ.
HyperMaze has five columns and six rows; Tileset has four cells. All are indexed.

The legacy formats provide no animation timing, so the conversions define no
timed clips. Invaders' `0.42` is draw scale, not a frame duration; apply it at
submission. Its alpha is one and placement offsets are zero. Centered sprite and
bottom-left tile pivots follow the recovered v2 loader conventions; Rover and
Tileset descriptions identify where their sources supplied no explicit anchor.
Duplicate `donotuse.dat` and the annotated, conflicting Rover draft do not create
additional runtime assets. Original data files remain available for comparison.
The font width conversion is separate [FFont compatibility metadata](../resources/legacy-ffont.md),
not a loadable graphics manifest.
