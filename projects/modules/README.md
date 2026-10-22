# Module roles and engine contracts

Modules are grouped by the engine role they provide. Each concrete owner defines
its primary library, public headers, dependencies and tests. The role folders
organize those owners; engine interfaces remain in `projects/engine/`.

Published module guides live in `docs/site/modules/`. Each module's `README.md`
is a relative symlink to its central guide.

```text
modules/
  runtime/
    startup/        # Application bootstrap and selected-module integration
    workers/        # Optional topology and JSON settings
  platform/
    native-glfw/    # Display, windows and input
    native-sdl3/    # Source-only scaffold; runtime adapters not implemented
    debug-terminal-linux/ # Optional process output viewer
  graphics/
    opengl/         # Rendering, presentation and graphics resources
  ui/
    tgui/           # TGUI toolkit integration
    rmlui/          # RmlUi toolkit integration
  audio/
    miniaudio/      # Audio decoding, mixing and native output
```

| Owner | Build target | Public alias | Root selection | Engine contracts |
| --- | --- | --- | --- | --- |
| [runtime/startup](../../docs/site/modules/startup.md) | `module_startup` | `Cheryl::Startup` | `CHERYL_BUILD_STARTUP` | Standard application bootstrap over Engine, with private configured backend integration. |
| [runtime/workers](../../docs/site/modules/workers.md) | `module_workers` | `Cheryl::Workers` | `CHERYL_BUILD_WORKERS` | Optional topology/JSON capabilities over Engine's core worker pools. |
| [platform/native-glfw](../../docs/site/modules/native-glfw.md) | `module_native_glfw` | `Cheryl::NativeGLFW` | `CHERYL_BUILD_NATIVE_GLFW` | [`core/display`](../engine/include/cheryl/core/display): `CE::Platform::DisplaySystem`, `Window`; [`core/controls`](../engine/include/cheryl/core/controls): `CE::Input::System`. |
| [platform/debug-terminal-linux](../../docs/site/modules/debug-terminal-linux.md) | `module_debug_terminal_linux` (interface) | `Cheryl::DebugTerminal::Linux` | `CHERYL_BUILD_DEBUG_TERMINAL_LINUX` | Optional startup callback through `Cheryl::DebugTerminal::Startup`; owns native process capture/viewer lifetime. |
| [graphics/opengl](../../docs/site/modules/opengl.md) | `module_opengl` | `Cheryl::OpenGL` | `CHERYL_BUILD_OPENGL` | [`core/rendering`](../engine/include/cheryl/core/rendering): `CE::Rendering::Renderer`, `PresentationSurface`; [`assets/resources`](../engine/include/cheryl/assets/resources): `Provider`, `Image`, `Buffer2D`, `Program`, `Pipeline`. |
| [ui/tgui](../../docs/site/modules/tgui.md) | `module_ui_tgui` | `Cheryl::UI::TGUI` | `CHERYL_BUILD_UI_TGUI` | Translates input and records/uploads retained scenes through Engine contracts. The toolkit owns its widget API. |
| [ui/rmlui](../../docs/site/modules/rmlui.md) | `module_ui_rmlui` | `Cheryl::UI::RmlUi` | `CHERYL_BUILD_UI_RMLUI` | Native document sessions, routed input and premultiplied retained rendering through Engine contracts. |
| [audio/miniaudio](../../docs/site/modules/miniaudio.md) | `module_audio_miniaudio` | `Cheryl::Audio::Miniaudio` | `CHERYL_BUILD_AUDIO_MINIAUDIO` | [`audio`](../engine/include/cheryl/audio): immutable `Clip`, synchronized `Voice` and application-owned `System`. |

Startup, Workers, Native GLFW, OpenGL, TGUI, RmlUi and miniaudio are selected by default.
The opt-in `Cheryl::NativeSDL3::Scaffold` interface target declares a future
platform boundary; it provides no runtime implementation or SDK component.
See the [SDL3 scaffold notes](../../docs/site/development/modules.md#sdl3-scaffold).
Miniaudio links only Engine and its private SDK, with no window or graphics
requirement.
The Linux Debug terminal defaults to `ON` for single-configuration Linux GNU/Clang
Debug builds. Explicit `ON` includes it in every configuration from that build
directory. Application/test composition selects it without adding a reverse Engine
dependency. Other platforms omit it.
OpenGL requires Native GLFW. Each UI owner links only Engine and its own toolkit;
either or both can be selected. For Engine alone, disable all modules, including
Startup and Workers; see the [engine-only configuration](../../docs/site/development/building.md#engine-only-configuration). Link the public targets to inherit their include directories and
dependency requirements; each owner also supplies a standalone CMake entry point.

Command-line applications link `Cheryl::Startup` and use the generic
`CE::Application::make_startup()` factory. `Cheryl::OpenGL::Startup` remains a
compatibility interface and produces no extra runtime binary. CLI11 remains
Startup's public header frontend; plain Engine/OpenGL consumers do not inherit
it. The [application guide](../../docs/site/development/consuming-engine.md#command-line-startup)
describes configuration and lifetimes.

An owner can fulfill several related contracts. Native GLFW couples display and
input lifetimes; OpenGL couples rendering and resource ownership with its context.
Their detailed contract maps live with those owners. Add another role folder when
an implemented module needs it.

Selection, standalone composition and test ownership are described in
[the module guide](../../docs/site/development/modules.md).
The [test guide](../../docs/site/development/testing.md) lists runner groups and
selection rules.
