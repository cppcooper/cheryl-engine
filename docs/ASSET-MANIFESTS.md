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

The current fixed `AnchorType` enum does not represent bottom center, so the loader/rendering update must retain the numeric pivot or extend vertex generation instead of rounding it to an existing anchor.

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

The Puny World manifest is transcribed from the author-supplied `punyworld-overworld-tiles.tsx`. Its bundled PNG is byte-for-byte identical to the source PNG, so its 70 animated targets, 280 timed frames, 168 corner-Wang assignments, and 45 edge-Wang assignments preserve the upstream tile IDs exactly.

## Engine implementation order

1. Discover only manifest JSON files in the `assets` directory and require supported `version` values.
2. Resolve each entry's texture and pivot, validate its grid against the decoded texture, and register its namespaced ID.
3. Materialize row-major cell UVs and named views.
4. Resolve `animation_profile` references and build sprite and animated-tile sequences.
5. Build Wang-signature and bitmask lookup tables, retaining duplicate Wang variants.
6. Fail a manifest atomically on bad paths, missing profiles, out-of-range cells/views, or invalid autotile terrain references.
