# C++ API

The reference is generated from the public headers in this documentation version.
It covers Cheryl-owned engine headers, startup and worker-topology support,
and the GLFW, OpenGL, TGUI, RmlUi and miniaudio modules. These are backend modules;
their availability depends on the targets enabled in your build.

## Namespaces

Namespaces group declarations by the work they do. Backend implementations live
beneath their portable domain; an asset family's metadata and operations live
beside that family. Repeated names such as `Asset`, `Definition`, and `Manager`
have a specific meaning within their owner.

| Domain | Responsibility |
| --- | --- |
| `CE::Application` | Command-line configuration, startup, and backend composition |
| `CE::Game` | Game hooks and update context |
| `CE::Runtime` | Live engine context, game sessions, and owner-thread dispatch; `Simulation` owns scheduling |
| `CE::Workers` | Reusable pools, groups, topology, and CPU placement; `Json` and `Hwloc` provide optional support |
| `CE::Input` | Physical controls and input transport; `Actions`, `Events`, `Capture`, and `Routing` organize semantic state and delivery |
| `CE::Assets` | Asset families (`Sprites`, `Tilesets`, `Graphics`, `Images`, `Shaders`, `Materials`), shared grids and animation, loading, manifests, and caches |
| `CE::Rendering` | Cameras, frames, draw packets, and portable `Geometry`, `Resources`, `Pipelines`, `Materials`, `Parameters`, `Sampling`, and `Submission` contracts |
| `CE::Rendering::OpenGL` | Concrete OpenGL resources, rendering, and native context integration |
| `CE::Platform` | Portable window/display contracts; `GLFW` owns native windows and input adapters |
| `CE::UI` | Toolkit adapters under `TGUI` and `RmlUi` |
| `CE::Text` | Font selection and shaping; `Rendering` owns prepared/uploaded text, `Legacy` owns older font formats, and `Numbers` owns numeric parsing |
| `CE::Memory` | Blocks, allocators, object pools, pointers, and byte-size/fit helpers |
| `CE::State`, `CE::Time`, `CE::Math`, `CE::Files` | State utilities, clocks/durations, packed bits, and file indexing |
| `CE::Audio`, `CE::Events`, `CE::Logging`, `CE::Diagnostics`, `CE::Exceptions` | Audio, generic event delivery, logging, diagnostics, and failures |

Input capture and routing are shared game/UI protocols. They do not depend on a
widget toolkit. Likewise, worker facilities do not require a running game session.
Namespace ownership and CMake link targets serve different purposes: these
portable facilities are still provided by `Cheryl::Engine`.

## Entry points

Include `<cheryl/cheryl.h>` and link `Cheryl::Cheryl` for the complete selected
assembly. Individual module umbrellas live under `cheryl/modules/`, and support
umbrellas under `cheryl/support/`. The [consumer guide](../development/consuming-engine.md#dependencies-and-headers)
explains source-checkout and SDK target selection.

| Task | Entry point | Declaring header |
| --- | --- | --- |
| Configure application startup | {cpp:class}`CE::Application::Startup` | [startup.h](../../projects/engine/support/startup/include/cheryl/core/engine/startup.h) |
| Implement game hooks | {cpp:struct}`CE::Game::AbstractGame` | [abstract-game.h](../../projects/engine/include/cheryl/core/game-framework/abstract-game.h) |
| Prepare and upload assets | {cpp:struct}`CE::Assets::Loading::Loader` | [asset-loader.h](../../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h) |
| Select immutable font data | {cpp:class}`CE::Text::FontCollection` | [font-collection.h](../../projects/engine/include/cheryl/text/font-collection.h) |
| Retain sprite resources | {cpp:struct}`CE::Assets::Sprites::Asset` | [sprite.h](../../projects/engine/include/cheryl/assets/types/2d/sprite.h) |
| Find loaded sprites | {cpp:struct}`CE::Assets::Sprites::Manager` | [sprite-mgr.h](../../projects/engine/include/cheryl/core/resources/asset-management/sprite-mgr.h) |

See {cpp:enum}`CE::Runtime::Simulation::UpdateKind` for simulation update kinds,
{cpp:type}`CE::Text::FontSource` for font selection sources, and
{cpp:struct}`CE::Assets::Cache::Store` for the retained-cache template inherited by
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

## Migration

This reorganization changes C++ source names and ABI. Rebuild consumers with the
matching engine version. Existing include paths and CMake target names remain
available; old names are not re-exported through compatibility aliases.

| Previous name | Current name |
| --- | --- |
| `CE::Assets::Sprite` / `SpriteDefinition` / `SpriteAnimation` / `SpriteMgr` | `CE::Assets::Sprites::Asset` / `Definition` / `Animation` / `Manager` |
| `CE::Assets::Tileset` / `TilesetDefinition` / `TileAnimation` / `TilesetMgr` | `CE::Assets::Tilesets::Asset` / `Definition` / `Animation` / `Manager` |
| `CE::Assets::Material` / `Pipeline` / `ResourceProvider` | `CE::Rendering::Resources::Material` / `Pipeline` / `Provider` |
| `CE::Engine::EngineContext` / `WorkerPool` / `Startup` | `CE::Runtime::Context` / `CE::Workers::Pool` / `CE::Application::Startup` |
| `CE::GFramework::AbstractGame` / `GameRuntime` | `CE::Game::AbstractGame` / `CE::Runtime::GameRuntime` |
| `CE::Input::ActionId` / `InputBindings` / `InputCapture` / `InputRouting` | `CE::Input::Actions::Id` / `Actions::Bindings` / `Capture::Collector` / `Routing::Manager` |
| `CE::RenderAPIs::iRenderer` / `OpenGLRenderer` | `CE::Rendering::Renderer` / `CE::Rendering::OpenGL::Renderer` |
| `CE::Assets::PreparedText` / `RenderedText` | `CE::Text::Rendering::Prepared` / `Generation` |
| `CE::Assets::Font` / `STBFont` / `FFont` | `CE::Text::Legacy::Font` / `AsciiFont` / `BitmapFont` |
| `CE::Mem`, `CE::Obj`, `CE::ptr` | `CE::Memory`, `Memory::Objects`, `Memory::Pointers` |
| Global state utilities / duration helpers / `FileMgr` | `CE::State` / `CE::Time` / `CE::Files::FileIndex` |

Replace broad imports of the former namespaces with qualified names or local
aliases for the family you use. For example:

```cpp
namespace Sprites = CE::Assets::Sprites;
auto sprite = Sprites::Manager::get().get_asset("first-game", "actor");
```

Family managers use `load()` and `reload()` where the family identifies the
resource kind. The shader catalogue uses `find_program()` and `find_material()`;
the image manager's decoded-image upload is `upload()`. The aggregate loading
entry point remains `CE::Assets::Loading::Loader::load_assets()`.

```{toctree}
:maxdepth: 2

Symbols <../generation/generated/api/library_root>
```
