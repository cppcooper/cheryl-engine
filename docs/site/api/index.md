# C++ API

The reference is generated from the public headers in this documentation version.
It covers Cheryl-owned Engine headers, public Startup/Workers modules,
and the GLFW, OpenGL, TGUI, RmlUi and miniaudio modules. Module
availability depends on the targets enabled in your build.

For a known symbol, use **Symbols** below or search its qualified name. For an
operation you have not chosen an API for, start with the
[Engine Systems tables](../engine-systems/index.md#find-a-system) or the
[task index](../getting-started/how-to/index.md), then return here for the exact
contract. [Migration](#migration) is for existing consumers moving from the older
names and binary boundaries; new projects can use the current entry points.

```{toctree}
:maxdepth: 2

Symbols <../generation/generated/api/library_root>
```

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

The [Engine Systems tables](../engine-systems/index.md#find-a-system) group the
public types and important enums by task, with detailed contract links. For
physical control names, start with [Input controls](../engine-systems/input.md#controls-for-bindings);
for backend implementations, start with the
[adapter catalogue](../architecture/interfaces-adapters.md).

Include `<cheryl/cheryl.h>` and link `Cheryl::Cheryl` for the complete selected
assembly. Individual module umbrellas live under `cheryl/modules/`, and support
umbrellas under `cheryl/support/`. The [consumer guide](../development/consuming-engine.md#dependencies-and-headers)
explains source-checkout and SDK target selection.

| Task | Entry point | Declaring header |
| --- | --- | --- |
| Configure application startup | {cpp:class}`CE::Application::Startup` | [startup.h](../../../projects/modules/runtime/startup/include/cheryl/application/startup.h) |
| Implement game hooks | {cpp:struct}`CE::Game::AbstractGame` | [abstract-game.h](../../../projects/engine/include/cheryl/core/game-framework/abstract-game.h) |
| Choose physical input controls | {cpp:enum}`CE::Input::KeyboardKey`, {cpp:enum}`CE::Input::MouseButton`, {cpp:enum}`CE::Input::GamepadControl` | [button-codes.h](../../../projects/engine/include/cheryl/core/controls/button-codes.h) |
| Attach reusable per-consumer action bindings | {cpp:class}`CE::Input::Actions::ContextSubmission`, {cpp:class}`CE::Input::Actions::Context` | [action-context.h](../../../projects/engine/include/cheryl/core/controls/action-context.h); [action contexts](../runtime/input-state-model.md#action-contexts) |
| Prepare and upload assets | {cpp:struct}`CE::Assets::Loading::Loader` | [asset-loader.h](../../../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h) |
| Configure the application's default and named fonts | {cpp:class}`CE::Text::FontService`, through `Context::fonts()` | [font-service.h](../../../projects/engine/include/cheryl/text/font-service.h); [configuration](../assets/text-layout.md#configure-fonts-once) |
| Select, save and restore a font choice | {cpp:class}`CE::Text::Font` and `FontService::choose_random`/`restore_or_choose` | [font.h](../../../projects/engine/include/cheryl/text/font.h); [saved choices](../assets/text-layout.md#choose-and-save-a-font) |
| Load a separate ordered font snapshot | {cpp:class}`CE::Text::FontCollection` | [font-collection.h](../../../projects/engine/include/cheryl/text/font-collection.h) |
| Retain and asynchronously replace a changing text label | {cpp:class}`CE::Text::Rendering::Updater`, {cpp:struct}`CE::Text::Rendering::Request` | [text-updater.h](../../../projects/engine/include/cheryl/text/text-updater.h); [retained text updates](../assets/text-layout.md#retained-text-updates) |
| Rasterize toolkit font glyphs on CPU | {cpp:class}`CE::Text::FontRasterizer` | [font-rasterizer.h](../../../projects/engine/include/cheryl/text/font-rasterizer.h) |
| Retain sprite resources | {cpp:struct}`CE::Assets::Sprites::Asset` | [sprite.h](../../../projects/engine/include/cheryl/assets/types/2d/sprite.h) |
| Draw a whole PNG without a sprite manifest | {cpp:struct}`CE::Assets::Graphics::Asset` | [graphic.h](../../../projects/engine/include/cheryl/assets/types/2d/graphic.h); [whole-image sequence](../getting-started/how-to/draw-sprite.md#draw-a-whole-image) |
| Find loaded sprites | {cpp:struct}`CE::Assets::Sprites::Manager` | [sprite-mgr.h](../../../projects/engine/include/cheryl/core/resources/asset-management/sprite-mgr.h) |

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

The SDK/Engine/Startup/Workers major version is 4; Native GLFW, OpenGL, TGUI and
RmlUi are major version 2 because their public layouts/implementation boundaries
changed. Audio and the static terminal attachment retain their component versions.
Rebuild consumers with one matched header/binary set. Startup/worker compatibility
headers/targets remain available where their capability is enabled; the older
C++ namespace names below are not re-exported as aliases.

Use the generic `CE::Application::make_startup()` and Cheryl-owned logging levels,
file callbacks, Engine input control enums/device snapshots and compiled OpenGL methods.
Private spdlog/Gainput/GLFW/GLAD implementation headers are no longer application
requirements. Native toolkit authoring remains part of the UI owners' public ABI.

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

### Input bindings

Bind Engine enums directly, for example
`bindings.bind_button({input.keyboard_id(), CE::Input::KeyboardKey::Escape}, quit)`.
The same form accepts mouse buttons and gamepad controls, including in chords.

| Previous interface | Current use |
| --- | --- |
| `CE::Platform::GLFW::keyboard_control(key)` / `mouse_control(button)` | Pass `CE::Input::KeyboardKey` / `CE::Input::MouseButton` directly. |
| `CE::Platform::GLFW::GamepadControl` constants | Use the scoped `CE::Input::GamepadControl` enum. Names include `DPadLeft`, `Back`, `Guide`, `LeftShoulder`, `LeftTriggerButton` and `LeftStick`; use `LeftTrigger` for its axis. Right-side equivalents follow the same naming. |
| `CE::Platform::GLFW::MouseControl` | Use `CE::Input::MouseControl` for absolute axes and wheel pulses as well as relative movement/scroll. |
| `CE::Platform::GLFW::InputMapper` | Access `CE::Input::Actions::Bindings` through `input.bindings()`. |

Remove includes of the deleted `glfw-bindings.h` and `input-mapper.h`; include
`<cheryl/core/controls/button-codes.h>` alongside the input interface. These
removed interfaces have no compatibility aliases. Direct foreign-enum bindings
fail at compile time; numeric casts of Gainput/GLFW constants do not produce
Engine control IDs. Keep raw `ControlId` values for explicitly defined adapter
extensions. The [binding task](../getting-started/how-to/bind-input.md) and
[control guide](../runtime/input-state-model.md#control-identities-and-binding-tokens)
show the current headers, validation and examples.
