# Asset package catalog

This catalog lists the artwork packages referenced by the checked-in JSON manifests
in `assets/`, with download sources and image placement. The manifests are tracked;
their package images are supplied separately. Download the named files from the
author pages below, then extract or copy the images into the expected locations.

## Located packages

| Package and author page | Download | Tracked manifests |
| --- | --- | --- |
| [Mini World Sprites — Shade, octoshrimpy](https://merchant-shade.itch.io/16x16-mini-world-sprites) | `MiniWorldSprites.zip` | [atlas.json](../../assets/atlas.json), [buildings-colored.json](../../assets/buildings-colored.json), [buildings-orc.json](../../assets/buildings-orc.json) |
| [MiniWorld Character Customizer — Shade](https://merchant-shade.itch.io/16x16-mini-world-sprites) | `Character-Customizer.zip` | [character-customizer.json](../../assets/character-customizer.json) |
| [Puny World — Shade](https://merchant-shade.itch.io/16x16-puny-world) | `punyworld-overworld-tileset.png` or `PUNY_WORLD_v1.zip` | [punyworld-overworld.json](../../assets/punyworld-overworld.json) |
| [Mage City Arcanos — Hyptosis](https://opengameart.org/content/mage-city-arcanos) | `magecity.png` | [magecity.json](../../assets/magecity.json) |
| [Dungeon tileset — Buch, with contributions from surt](https://opengameart.org/content/dungeon-tileset) | [dungeon_tiles.png](https://opengameart.org/sites/default/files/dungeon_tiles_0.png) | [dungeon_tiles.json](../../assets/dungeon_tiles.json) |

The three MiniWorld sprite/building manifests share one archive. Character Customizer
is a separate archive on the same page. That page also offers `MiniworldGuide.docx`
for the artwork's animation documentation.

## Image placement

All paths below are relative to the checkout's `assets/` directory. Preserve case,
spaces and accented filenames when extracting; avoid adding an extra archive-wrapper
directory between `assets/` and the paths in the manifests.

### MiniWorld sprites

Place the archive's `MiniWorldSprites/` tree under `assets/`, preserving its internal
directories. `atlas.json` references individual sheets in `Animals/`, `Buildings/`,
`Characters/`, `Ground/`, `Miscellaneous/`, `Nature/`, `Objects/` and `User Interface/`.
The other two manifests reference these combined sheets from the same package:

- `MiniWorldSprites/ColoredBuildingsPreview.png` for `buildings-colored.json`.
- `MiniWorldSprites/Buildings/Enemy/Orc/AllBuildings-Preview.png` for `buildings-orc.json`.

The combined sheets are required alongside the individual sheets; the top-level
`AllAssetsPreview.png` does not replace them.

### Character Customizer

Place the flattened image at
`char-customizer/Character-Customizer-Photoshop.png`. The current manifest describes
an 80×320-pixel sheet arranged as five columns and twenty rows of 16-pixel cells.
If working from the package's layered Gimp/Photoshop file, export the matching image
as PNG with transparency and the original canvas dimensions. Keep the editor files
and package readme separately if needed for further customization.

### Puny World

Place `punyworld-overworld-tileset.png` at
`tilesets/punyworld-overworld-tileset.png`. The sheet is 432×1040 pixels, with
27 columns and 65 rows of 16-pixel cells. The full archive also supplies Tiled source
metadata, including `punyworld-overworld-tiles.tsx`; the manifest's animation and Wang
tables follow that layout, as described in the [manifest guide](asset-manifests.md#autotiles).

### Mage City Arcanos

Place the downloaded `magecity.png` at `tilesets/magecity.png`. Preserve the original
256×1450-pixel image. The manifest describes its populated region as eight columns
and forty-four rows of 32-pixel cells.

### Dungeon tileset

Place Buch's `dungeon_tiles.png` at `tilesets/dungeon_tiles.png`. The hosted download
can be saved as `dungeon_tiles_0.png`; rename it to the manifest's filename. The image
is 368×384 pixels, with twenty-three columns and twenty-four rows of 16-pixel cells.
This source PNG matches the existing sheet. The separate 0x72 packages below have
their own layouts and future manifests.

## Downloaded packages awaiting manifests

These source packages are saved locally under the ignored `asset-downloads/` tree,
outside `assets/` because the loader discovers PNGs recursively. The ZIPs remain
unextracted. `asset-downloads/0x72/sources.json` records their source pages, filenames,
sizes and SHA-256 hashes. Local downloads are not included in Git.

| Package and author page | Directory under `asset-downloads/0x72/` | Saved files |
| --- | --- | --- |
| [16x16 Dungeon Tileset — 0x72](https://0x72.itch.io/16x16-dungeon-tileset) | `16x16-dungeon-tileset/` | `0x72_16x16DungeonTileset.v5.zip` |
| [16x16 DungeonTileset II — 0x72](https://0x72.itch.io/dungeontileset-ii) | `dungeontileset-ii/` | `0x72_DungeonTilesetII_v1.7.zip`, `pumpkin_dude.png`, `doc.png` |

Unpacking these packages and writing a manifest for each belongs to the
[long-term asset task](../planning/long-term-plan.md#additional-dungeon-asset-manifests).
The extension and remix links on their author pages are separate candidate packages.
