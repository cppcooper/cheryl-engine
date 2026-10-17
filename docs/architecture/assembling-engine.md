# Assembling the Engine

Choose the public entry point for your application, then construct a compatible
set of implementations. [Engine Systems](../engine-systems/index.md) introduces
their responsibilities; the [module guides](../modules/index.md) describe the
available implementations.

## Facades and public includes

| Entry point | Use it to |
| --- | --- |
| `Cheryl::Cheryl` | Link the complete configured assembly through one CMake interface target. It forwards usage requirements to the existing libraries; it creates no combined binary. |
| `<cheryl/cheryl.h>` | Include the public headers enabled by your linked targets. This umbrella includes declarations; target selection supplies libraries and feature definitions. |
| `CE::Application::make_startup()` | Use the standard configuration, validation and backend-composition path instead of assembling its callbacks yourself. |

For the complete selected assembly:

```cmake
target_link_libraries(game PRIVATE Cheryl::Cheryl)
```

Link individual public targets when you need a narrower set. The same umbrella
then exposes that selection. Source assemblies can include the optional Linux
Debug terminal; installed SDK assemblies omit that source-only attachment.
The [source and SDK consumer guide](../development/consuming-engine.md)
provides complete CMake examples and deployment requirements.

## Factories and context composition

With Startup's OpenGL integration and native input selected, an application's
`main()` can use the standard factory:

```cpp
auto startup = CE::Application::make_startup("My game");
auto result = startup.initialize(argc, argv);
if (!result.should_start())
    return result.exit_code;

MyGame game(*result.context);
auto runtime = result.make_runtime(game);
runtime.run();
```

Include `<cheryl/application/startup.h>`. Here `MyGame` implements
`CE::Game::AbstractGame`; keep `startup`, `result` and `game` alive through the
session. The [Startup guide](../modules/startup.md) covers initial defaults,
application options and supplied services. Help and configuration errors return
before backend construction; other backend failures propagate.

For a custom assembly, supply `StartupBackend::create` or construct the lower-level
context directly. A factory returns owned components with compatible platform and
graphics domains; initialization and cleanup retain their owner-thread requirements.

`CE::Runtime::Context` owns the selected display, presentation, renderer and
resource provider, and owns or explicitly borrows input. `GameRuntime` borrows
that context and the game for one session; `run()` initializes and closes the
adapters around the game hooks. Use the [interface catalogue](interfaces-adapters.md)
when supplying a new implementation.

`make_startup()` selects its compiled default backend; it does not discover
runtime plugins. The optional [Linux Debug terminal](../modules/debug-terminal-linux.md)
attaches at process startup outside this context graph. Its support hook invokes
the application unchanged when no implementation callback is supplied.

## Backend boundaries

| Boundary | Contract |
| --- | --- |
| CPU asset data | Vertex layouts, indexed sprite/tileset and shader recipes, grid generation, owned RGBA pixels and shader-byte snapshots have no graphics-context dependency. Loader preparation runs independently of a provider. |
| Display and presentation | `CE::Platform::DisplaySystem` owns a selected window implementation. `CE::Platform::Window` and monitor snapshots carry neutral data; `CE::Rendering::PresentationSurface` presents. GLFW handles remain inside the platform/backend implementation. The renderer does not own the display. |
| Input | `CE::Input::System` supplies State plus independently requested ordered Events/Text. GLFW/Gainput translation stays in its adapter; games consume tick values and routed views. The default factory owns input; an explicit-reference overload borrows it. |
| Draw and material | Simulation writes complete ordered `CE::Rendering::Frame` passes. CPU submission helpers resolve sprite, tile, graphic, and glyph packets retaining geometry/material generations and copied values. `CE::Rendering::OpenGL::PipelineBindings` maps contract keys to native names; each pipeline applies full fixed state. |
| Resource creation and caches | `CE::Rendering::Resources::Provider` uploads decoded pixels and transient vertex spans, creates font atlases, and links programs. Optional sampler and shader-recipe builders reject unsupported requests. Cache readers retain handles under shared locks; construction and retired-handle destruction stay outside locks. One provider/loading thread binds the singleton caches until teardown. |
| Composition and scheduling | One runtime session owns platform polling and presentation. Concurrent mode adds one simulation worker and three reusable frame slots. `platform_dispatcher().submit()` transfers owned resource requests with future results; shutdown cancels pending requests before game cleanup. |
| OpenGL lifetime | Active resource use requires the owner thread and its actual current context. Handles retire without OpenGL calls from their destructors. Renderer shutdown restores its context, deletes tracked handles, closes their lifetime, and releases the context. Failed destructor cleanup invalidates retained handles. |
| Portable integration | The in-memory display/window, input, presentation, renderer, and resource provider exercise both runtime modes without OpenGL/GLFW headers. Aggregate regressions cover lifecycle, dispatch, cache publication, materials, preparation, and input routing. |

## Using the assembled context

Use `engine.display()`, `engine.window()`, and `engine.surface()` for the selected
platform and presentation contracts.
Use `engine.resources()` for backend asset creation after renderer initialization.
Construct an owned `CE::Assets::Loading::Loader(root)` for each root. `manifests()` returns a retained
immutable snapshot; singleton compatibility rejects a different root after its
first initialization.

The [loading guide](../assets/asset-loading.md) owns preparation/publication and
application bootstrap; the [render guide](../rendering/pipelines-and-materials.md)
owns program/material construction and packet resolution. Optional
[audio](../runtime/audio.md) is application-owned outside the runtime adapter graph.

## Execution and shutdown

Games request shutdown from `update()` with `tick.request_stop()`. The tick carries
the runtime's shared stop state, so games need no runtime singleton or callback
wiring. Applications holding the runtime can still call `CE::Runtime::GameRuntime::stop()`;
both requests use the same normal cleanup sequence. The stop request and tick
lifetime contracts are in [runtime-frame-boundary.md](../runtime/runtime-frame-boundary.md).

[Threads and Data Flow](threads-data-flow.md) explains owner dispatch, context-tracked
worker groups and the separate simulation thread. [Ownership and Cleanup](ownership-cleanup.md)
connects producer quiescence, accepted-work completion and game/adapter release;
the detailed runtime lifecycle owns their exact ordering and failure behavior.

## Current limits

The [status inventory](../STATUS.md) records source-backed capabilities and known
gaps at its review baseline. The [planning catalogue](../planning/README.md) links
their owning plans; each subject guide defines its current capability limits.
