# Asset loading

Asset loading is split into a document layer and explicit asset-type construction paths. `ManifestLoader` parses and semantically validates manifest 1.0 without requiring OpenGL. `Loader` discovers manifests and files, validates cross-manifest state and texture dimensions, then dispatches typed definitions to the texture, sprite, and tileset managers.

## Runtime entry point

Call the loader only after the engine has created an OpenGL context:

```cpp
auto& loader = CE::Assets::Loader::get("assets");
loader.load_assets();

auto actor = CE::Assets::SpriteMgr::get().get_asset(
    "miniworld:soldier_swordsman_cyan");
auto terrain = CE::Assets::TilesetMgr::get().get_asset(
    "punyworld:overworld");
```

The loader reads only JSON files directly inside the supplied asset root. It does not treat `assets/schemas/*.json` as manifests. Texture paths are resolved relative to their manifest, while sprites and tilesets are registered by `namespace:key` rather than by manifest filename.

The load order is:

1. Parse every manifest into typed, GL-independent definitions.
2. Reject duplicate asset IDs, missing textures, corrupt image metadata, and grids outside decoded texture bounds before constructing assets.
3. Load referenced and standalone PNG textures.
4. Construct every sprite and tileset grid from its resolved numeric pivot.
5. Load fonts and every supported shader stage.

`Loader::manifests()` retains the resolved documents for inspection after a successful load. A failed parse does not replace that collection.

## Typed manifest data

`assets/manifest.h` is the shared contract between parsing, asset construction, and later engine systems. It retains:

- grid origins, frame sizes, spacing, and row-major cell addressing;
- arbitrary normalized pivots, named views, and orientation cells;
- explicit timed animations and expanded profile animations with facings;
- animated-tile targets and per-frame durations;
- Wang terrain metadata, weighted tile variants, and signature lookup tables;
- four- and eight-neighbor bit orders and mask-to-cell lookup tables.

`Sprite::definition()` and `Tileset::definition()` expose this metadata after GPU construction. `Sprite::animation(name, facing)` returns a timed sequence; non-looping sequences clamp at their final frame. `Tileset::animation(name)` and `Tileset::animation_for(target)` provide the corresponding animated-tile sequences. Named views, orientations, and autotiles also have direct lookup methods.

For tooling or tests that do not have a graphics context, use `ManifestLoader::load(file)` or `ManifestLoader::parse(stream, source)` directly. The `asset-manifest-tests` CMake target exercises this path without a window-system dependency.

## Remaining engine integration

The loader now preserves and exposes the complete manifest semantics. Runtime consumers still need to decide *when* to select or advance them:

1. Add an animation controller that accumulates elapsed time using each frame's duration and respects the sequence's loop flag.
2. In the tile-map renderer, derive a Wang signature or bitmask from neighboring terrain, choose among matching weighted variants, then apply `animation_for()` if the selected cell is an animated target.
3. Decide which gameplay systems consume named views and orientation cells; the loader intentionally does not assign gameplay meaning beyond the manifest.
4. Add unloading or hot-reload behavior if the engine needs asset-root changes after initial startup. Repeated loads currently preserve already-registered manager entries.

These are rendering/game-state decisions rather than missing parsing or asset-construction work.
