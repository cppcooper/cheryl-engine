# Unresolved asset-manifest metadata

The checked-in manifests encode the available grids, pivots, animation profiles,
clips, and autotile tables. These remaining items need matching source metadata or
an artwork-owner decision before additional semantics can be encoded safely.

- MiniWorld character sheets other than the five Swordsman variants: obtain action-
  row, facing, and timing documentation before assigning profiles. Similar dimensions
  alone do not establish the Swordsman layout.
- MiniWorld directional weapons/projectiles: name orientation cells and choose spin/
  flight timings once their intended runtime behavior is decided. Grids and center
  pivots are already encoded.
- Buch dungeon sheet: locate metadata matching the 23×24 grid of 16-pixel cells
  (368×384 occupied pixels), or explicitly replace the sheet and remap it. Metadata
  for a different revision/layout cannot safely supply this sheet's named slices.
  The [download catalog](../../assets/catalog.md#dungeon-tileset) identifies its source.
- Colored/Orc buildings and Mage City: add per-cell gameplay names only if the engine
  needs semantic lookup below the existing regional/color view level.
- Character Customizer: decide whether runtime customization loads composited exports
  or retains selectable layers. The current manifest selects a flattened PNG; layer
  selection needs a different asset/runtime contract.

- Recovered Invaders and Rover: the original files supply frame ordering and
  geometry, but no durations or verified action names. Do not infer animation timing
  or material/FX intent. Invaders' draw scale is a submission setting; irregular
  Rover frames remain separately addressable entries. The
  [recovery contract](../../assets/asset-manifests.md#recovered-college-definitions)
  records pivot evidence and retained originals.

Tile selection uses the declared rules;
the [selection contract](../../assets/asset-values-and-playback.md#tile-selection)
does not infer missing artwork semantics.
Additional downloaded dungeon packages await separate manifests in the
[long-term plan](README.md#additional-dungeon-asset-manifests).
