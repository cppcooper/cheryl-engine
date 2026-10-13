# C++ API

The reference is generated from the public headers in this documentation version.
The initial extraction covers the game lifecycle, asset loading, fonts, sprites,
and the public base interfaces those examples use.

| Task | Entry point | Declaring header |
| --- | --- | --- |
| Implement game hooks | {cpp:struct}`CE::GFramework::AbstractGame` | [abstract-game.h](../../projects/engine/include/cheryl/core/game-framework/abstract-game.h) |
| Prepare and upload assets | {cpp:struct}`CE::Assets::Loader` | [asset-loader.h](../../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h) |
| Select immutable font data | {cpp:class}`CE::Text::FontCollection` | [font-collection.h](../../projects/engine/include/cheryl/text/font-collection.h) |
| Retain sprite resources | {cpp:struct}`CE::Assets::Sprite` | [sprite.h](../../projects/engine/include/cheryl/assets/types/2d/sprite.h) |
| Find loaded sprites | {cpp:struct}`CE::Assets::SpriteMgr` | [sprite-mgr.h](../../projects/engine/include/cheryl/core/resources/asset-management/sprite-mgr.h) |

See {cpp:enum}`CE::GFramework::UpdateKind` for simulation update kinds,
{cpp:type}`CE::Text::FontSource` for font selection sources, and
{cpp:struct}`CE::Assets::AssetMgr` for the retained-cache template inherited by
the sprite manager. The generated member lists include public overloads and
inherited interfaces; private implementation members are excluded.

Use the [asset-loading guide](../assets/asset-loading.md),
[sprite and playback guide](../assets/asset-values-and-playback.md), and
[text guide](../assets/text-layout.md) for ownership and operation sequences.

```{toctree}
:maxdepth: 2

generated/library_root
```
