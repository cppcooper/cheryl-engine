# Remaining asset metadata TODO

These items are not blockers for loading every texture/grid, applying pivots, playing the verified animations, or using the Puny World autotiles. They need matching source metadata or an artwork-owner decision before more semantics can be encoded safely.

- MiniWorld character sheets other than the five Swordsman variants: obtain verified action-row, facing, and timing documentation before assigning profiles. Similar dimensions alone are not enough to assume the Swordsman layout.
- MiniWorld directional weapons/projectiles: name orientation cells and choose any spin/flight timings once their intended runtime behavior is decided. Their grids and center pivots are already present.
- Legacy 0x72 dungeon sheet: locate metadata for the bundled 368×384 revision, or explicitly replace it with a newer release and remap it. The current upstream v5 sheet is 512×256, so its named slices cannot safely be copied onto this older layout.
- Colored/Orc buildings and Mage City: the physical grids and verified regional/color views are complete; add per-cell gameplay names only if the engine needs semantic lookup below the view level.
- Character Customizer: decide whether runtime customization should load composited exports or retain selectable layers. The runtime PNG is flattened, while the bundled XCF is the authoritative layered artwork.

No additional inspection is needed for the Puny World animations/Wang sets, the Swordsman animation profile, or the manifest pivot values; those are encoded now.
