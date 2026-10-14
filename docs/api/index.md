# C++ API

The reference is generated from the public headers in this documentation version.
It covers Cheryl-owned engine headers, startup and worker-topology support,
and the GLFW, OpenGL, TGUI, RmlUi and miniaudio modules. These are backend modules;
their availability depends on the targets enabled in your build.

| Task | Entry point | Declaring header |
| --- | --- | --- |
| Configure application startup | {cpp:class}`CE::Engine::Startup` | [startup.h](../../projects/engine/support/startup/include/cheryl/core/engine/startup.h) |
| Implement game hooks | {cpp:struct}`CE::GFramework::AbstractGame` | [abstract-game.h](../../projects/engine/include/cheryl/core/game-framework/abstract-game.h) |
| Prepare and upload assets | {cpp:struct}`CE::Assets::Loader` | [asset-loader.h](../../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h) |
| Select immutable font data | {cpp:class}`CE::Text::FontCollection` | [font-collection.h](../../projects/engine/include/cheryl/text/font-collection.h) |
| Retain sprite resources | {cpp:struct}`CE::Assets::Sprite` | [sprite.h](../../projects/engine/include/cheryl/assets/types/2d/sprite.h) |
| Find loaded sprites | {cpp:struct}`CE::Assets::SpriteMgr` | [sprite-mgr.h](../../projects/engine/include/cheryl/core/resources/asset-management/sprite-mgr.h) |

See {cpp:enum}`CE::GFramework::UpdateKind` for simulation update kinds,
{cpp:type}`CE::Text::FontSource` for font selection sources, and
{cpp:struct}`CE::Assets::AssetMgr` for the retained-cache template inherited by
the sprite manager. Derived pages show their declarations and explicit imported
overloads, then link to base pages for inherited interfaces. Read inherited
function templates on their declaring base so their parameters and lookup rules
remain accurate. Private implementation members are excluded.

Native input declarations are shown with `CHERYL_NATIVE_INPUT=1`; enable that
feature when using its adapters or the startup overload that owns native input.
Debug-terminal startup is also documented, but is available only to build-tree
consumers through `Cheryl::DebugTerminal::Startup`, rather than the installed SDK.

The generated signatures preserve parameter types, defaults and common function
qualifiers. Requires clauses and `final`/`[[nodiscard]]` annotations appear beneath
their declarations. The linked source headers provide the complete declarations.

Use the [asset-loading guide](../assets/asset-loading.md),
[sprite and playback guide](../assets/asset-values-and-playback.md), and
[text guide](../assets/text-layout.md) for ownership and operation sequences.
The [first-game walkthrough](../getting-started/first-game.md) connects these APIs
in a maintained application.

```{toctree}
:maxdepth: 2

Symbols <generated/library_root>
```
