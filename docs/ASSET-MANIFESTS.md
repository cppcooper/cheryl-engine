# Asset manifest 1.0

The authoritative format is `assets/schemas/asset-manifest-1.0.schema.json`. Manifest files live directly in `assets/`; schema files are not asset manifests and must not be discovered as loadable assets.

## Identity and inheritance

- A globally unique asset ID is `namespace` plus the sprite or tileset map key. The engine should preserve both components rather than relying on filenames.
- An entry's `texture` overrides the manifest-level `texture`. One of those fields is required by the schema.
- An entry's `pivot` overrides `defaults.sprite.pivot` or `defaults.tileset.pivot` according to the containing map.
- Texture paths are forward-slash, case-sensitive paths relative to the manifest file.

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

Per-sprite `animations` support arbitrary cells and per-frame durations. Tileset animations additionally identify a `target` cell: rendering that target advances through its `frames`. If an autotile selects an animated target, autotile selection happens first and animation substitution happens second.

## Autotiles

Wang signatures always use this explicit order:

```text
north, north_east, east, south_east,
south, south_west, west, north_west
```

Terrain ID `0` means no terrain. `wang-corner` sets use the corner slots and `wang-edge` sets use the edge slots. Multiple cells may share a signature; they are visual variants, selected uniformly unless a `weight` is present.

The schema also supports `four-neighbor` and `eight-neighbor` bitmask autotiles. `bit_order[i]` owns bit `1 << i`, and the decimal mask string selects a cell from `cases`.

The Puny World manifest is transcribed from the author-supplied `punyworld-overworld-tiles.tsx`. The corresponding source-bundle PNG was verified byte-for-byte against the upstream PNG, so its 70 animated targets, 280 timed frames, 168 corner-Wang assignments, and 45 edge-Wang assignments preserve the upstream tile IDs exactly.

## Runtime implementation

The manifest parser, typed asset dispatch, pivot/grid construction, animation expansion, and autotile lookup data are implemented. See [`ASSET-LOADING.md`](ASSET-LOADING.md) for the runtime entry point, validation/load order, retrieval APIs, and the remaining animation-controller and tile-map-renderer integration work.
