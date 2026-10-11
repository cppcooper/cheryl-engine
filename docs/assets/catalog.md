# Asset package catalog

The `assets/` Git submodule contains graphics definitions, shaders, UI fixtures and
recovered college assets. Initialize its pinned revision with the checkout's
[submodule setup](../../README.md#setup). This catalog lists optional external
artwork packages and their image placement; those package PNGs are supplied
separately. Definitions live under `graphics/definitions/`, images under
`graphics/textures/`. The [manifest guide](asset-manifests.md) owns index selection
and graphics-relative paths.

## Located packages

| Package and author page | Download | Tracked manifests |
| --- | --- | --- |
| [Mini World Sprites — Shade, octoshrimpy](https://merchant-shade.itch.io/16x16-mini-world-sprites) | `MiniWorldSprites.zip` | [atlas.json](../../assets/graphics/definitions/MiniWorldSprites/atlas.json), [buildings-colored.json](../../assets/graphics/definitions/MiniWorldSprites/buildings-colored.json), [buildings-orc.json](../../assets/graphics/definitions/MiniWorldSprites/Buildings/Enemy/Orc/buildings-orc.json) |
| [MiniWorld Character Customizer — Shade](https://merchant-shade.itch.io/16x16-mini-world-sprites) | `Character-Customizer.zip` | [character-customizer.json](../../assets/graphics/definitions/char-customizer/character-customizer.json) |
| [Puny World — Shade](https://merchant-shade.itch.io/16x16-puny-world) | `punyworld-overworld-tileset.png` or `PUNY_WORLD_v1.zip` | [punyworld-overworld.json](../../assets/graphics/definitions/tilesets/punyworld-overworld.json) |
| [Mage City Arcanos — Hyptosis](https://opengameart.org/content/mage-city-arcanos) | `magecity.png` | [magecity.json](../../assets/graphics/definitions/tilesets/magecity.json) |
| [Dungeon tileset — Buch, with contributions from surt](https://opengameart.org/content/dungeon-tileset) | `dungeon_tiles.png` | [dungeon_tiles.json](../../assets/graphics/definitions/tilesets/dungeon_tiles.json) |

The three MiniWorld sprite/building manifests share one archive. Character Customizer
is a separate archive on the same page. That page also offers `MiniworldGuide.docx`
for the artwork's animation documentation.

## Image placement

All image paths below are relative to the checkout's `assets/graphics/textures/` directory. Preserve
case, spaces and accented filenames when extracting; avoid adding an extra archive-wrapper
directory between `assets/graphics/textures/` and the package paths listed below.

### MiniWorld sprites

Place the archive's `MiniWorldSprites/` tree under `assets/graphics/textures/`, preserving its
internal directories. `atlas.json` references individual sheets in `Animals/`, `Buildings/`,
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

## Packages awaiting manifests

These packages need separate manifests for their layouts. Keep original downloads
in a staging directory outside the runtime asset roots.
The ignored `asset-downloads/0x72/` tree is suitable for local staging. A local
`sources.json`, when available, records source pages, filenames, sizes and hashes;
downloads and provenance records are not included in Git.

| Package and author page | Staging directory under `asset-downloads/0x72/` | Source files |
| --- | --- | --- |
| [16x16 Dungeon Tileset — 0x72](https://0x72.itch.io/16x16-dungeon-tileset) | `16x16-dungeon-tileset/` | `0x72_16x16DungeonTileset.v5.zip` |
| [16x16 DungeonTileset II — 0x72](https://0x72.itch.io/dungeontileset-ii) | `dungeontileset-ii/` | `0x72_DungeonTilesetII_v1.7.zip`, `pumpkin_dude.png`, `doc.png` |

Unpacking these packages and writing a manifest for each belongs to the
[long-term asset task](../planning/long-term/README.md#additional-dungeon-asset-manifests).
The extension and remix links on their author pages are separate candidate packages.

## Recovered college assets

The submodule also contains [Invaders, HyperMaze, Rover and Tileset definitions](asset-manifests.md#recovered-college-definitions),
locally populated images under `graphics/textures/misc/` and
`graphics/textures/tilesets/`, and tracked [bitmap-font inputs](../resources/legacy-ffont.md)
under `graphics/fonts/`. The legacy sprite/tile PNGs follow the same ignored local
image policy as external packages; checking out definitions does not supply them.
These are recovered project assets, not newly sourced or licensed packages. Their
original numeric definitions remain beside the JSON conversions. The submodule's
[provenance notice](../../assets/README.md) still applies where credits or sources
are unknown. Conversion alone does not establish native rendering acceptance.
