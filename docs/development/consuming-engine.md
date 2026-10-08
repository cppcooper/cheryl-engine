# Consuming the engine

The supported packaging boundary is build-tree composition from a Cheryl checkout.
`projects/engine/` owns the neutral `Cheryl::Engine` target, backed by the build
target `cengine` and archive output name `cheryl-engine`. Applications link selected
integration targets explicitly; [the module guide](modules.md) describes ownership,
standalone composition and tests.

```cmake
set(CHERYL_BUILD_TESTS OFF)
set(CHERYL_BUILD_DEMO OFF)
set(CHERYL_BUILD_NATIVE_GLFW OFF)
set(CHERYL_BUILD_OPENGL OFF)
set(CHERYL_BUILD_UI_TGUI OFF)
set(CHERYL_BUILD_UI_RMLUI OFF)
set(CHERYL_BUILD_AUDIO_MINIAUDIO OFF)
add_subdirectory(path/to/cheryl-engine cheryl)
add_executable(application main.cpp)
target_link_libraries(application PRIVATE Cheryl::Engine)
```

For native graphics, enable Native GLFW, OpenGL and native input, then link
`Cheryl::Engine`, `Cheryl::NativeGLFW` and `Cheryl::OpenGL`. Select and link
`Cheryl::UI::TGUI`, `Cheryl::UI::RmlUi` or `Cheryl::Audio::Miniaudio` when needed.
For command-line startup, link `Cheryl::OpenGL::Startup`; it supplies the common
startup target and the native/graphics targets transitively.
Consumers obtain C++23, includes, logging policy and SDK requirements through
targets rather than compiling Engine sources or repeating dependency lists.

## Command-line startup

[`CE::Engine::Startup`](../../projects/engine/support/startup/include/cheryl/core/engine/startup.h)
owns CLI11 parsing and validates execution, input polling and simulation timing
before invoking a selected backend factory. Link `Cheryl::Startup` for a custom
backend or `Cheryl::OpenGL::Startup` for the
[GLFW/OpenGL factory and options](../../projects/modules/graphics/opengl/README.md#command-line-startup).
The support targets expose CLI11 in their public headers; linking Engine or OpenGL
alone retains their direct context/runtime construction APIs without that public
dependency.

Create startup on the platform owner, register application options with
`add_application_options`, then call `initialize(argc, argv)` once. Option-definition
callbacks run immediately; any values bound to the parser must remain at a stable
address through initialization. Startup itself cannot be copied or moved because
its callbacks bind its configuration.

Help and parse/configuration errors produce a `StartupResult` with no engine.
Return its `exit_code` when `should_start()` is false; help succeeds and invalid
arguments fail before native context construction. A successful result owns the
`EngineContext` and the parsed `RuntimeConfiguration`. Construct the game from that
context, then call `result.make_runtime(game)` to apply its run mode, polling and
timing together. Keep the result and game alive through runtime destruction;
`make_runtime` requires an lvalue result. Backend construction failures propagate,
and `run()` retains the runtime's existing initialization/cleanup contract.

`StartupConfiguration` supplies application defaults through `execution` and
`runtime`. The common Engine options override those defaults:

| Argument | Effect |
| --- | --- |
| `--concurrent` | Selects a separate simulation owner thread. |
| `--fixed` | Selects fixed-step simulation. |
| `--fixed-step-ms=N` | Selects fixed simulation and sets its positive step in milliseconds. |
| `--variable-interval-ms=N` | Sets variable-update spacing; zero permits unpaced updates. |
| `--max-fixed-updates=N` | Sets a positive limit on ordinary fixed updates per scheduler turn. |
| `--variable-catch-up` | Selects fixed simulation with bounded variable catch-up recovery. |
| `--recovery-prefix=N` | Sets the fixed-update prefix before recovery, at most `max_fixed_updates`. |
| `--recovery-cap-ms=N` | Caps the recovery duration; zero removes the cap. |
| `--input-unlimited` | Selects unlimited completed polls between simulation updates. |
| `--input-capacity=N` | Selects finite input polling with a positive completed-poll capacity. |
| `--input-spacing-ms=N` | Sets minimum spacing between completed polls; zero removes the delay. |
| `--worker-count=N` | Sets capacity for the lazily created Engine CPU pool, separate from the simulation thread. Owned capacity must be positive; an injected shared pool retains its own capacity. |

Numeric values are nonnegative and must satisfy the linked
[timing](../runtime/simulation-timing.md) and
[polling](../runtime/input-state-model.md#polling-backlog-and-scheduling) contracts.
Option callbacks apply in parse order: `--input-unlimited --input-capacity=4`
selects finite polling; reversing them selects unlimited polling. Without overrides,
the default configuration is sequential variable simulation, lockstep polling and
one lazy CPU worker.

A custom `StartupBackend` provides a required `create(ExecutionOptions)` factory,
optional Backend argument definitions and optional validation. The factory must
return an owned context; a null result fails startup. Backend selection is currently
programmatic. CLI backend selection, application-preference/build-default resolution
and runtime-loaded discovery remain unimplemented at the selection point after
parsing. [The demo option helper](../../projects/apps/demo/src/demo-options.cpp)
shows application-specific registration without duplicating the engine parser.

## Game hooks and ownership

Implement `CE::GFramework::AbstractGame` and lend it, with an `EngineContext`, to
`GameRuntime`. Keep both alive through `run()` returning or throwing. The selected
factory assembles display, window, input, presentation, renderer and resources;
`run()` initializes those adapters before calling the game.

| Hook | Owner | Responsibility |
| --- | --- | --- |
| `init()` | Platform/graphics | Bind actions and create initial resources after adapter startup. |
| `update(tick)` | Simulation | Advance game state using `delta_seconds` and the tick's input/copy of window sizes. |
| `prepare_render_frame(writer) const` | Simulation | Resolve complete ordered passes/packets; publish no borrowed game state. |
| `quiesce()` | Platform after simulation joins | Stop external producers and invalidate borrowed listeners; keep their dependencies alive. |
| `deinit()` | Platform/graphics | Release application resources after accepted work settles and frames recycle. |

Sequential mode runs all hooks on the calling thread. Concurrent mode gives
update/frame preparation to one simulation worker; platform polling, uploads and
presentation remain on the caller. Cleanup pairs with attempted initialization,
so hooks must tolerate partial startup. The [frame boundary](../runtime/runtime-frame-boundary.md)
defines shutdown and failure ordering.

### Minimal native application

With Native GLFW, OpenGL and native input enabled, this opens a window and stops
when Escape is pressed. It submits no draws yet:

```cpp
#include <cheryl/backends/opengl/startup.h>
#include <cheryl/core/controls/input-interface.h>
#include <cheryl/core/game-framework/abstract-game.h>
#include <cheryl/core/game-framework/game-runtime.h>
#include <gainput/gainput.h>

class Game final : public CE::GFramework::AbstractGame {
    CE::Engine::EngineContext& engine_;
    static constexpr CE::Input::ActionId quit_{1};

public:
    explicit Game(CE::Engine::EngineContext& engine) : engine_(engine) {}
    void init() override {
        auto& input = engine_.input();
        static_cast<void>(input.bindings().bind_button({input.keyboard_id(), gainput::KeyEscape}, quit_));
    }
    void update(const CE::GFramework::TickContext& tick) override {
        if (tick.input.button(quit_).pressed())
            tick.request_stop();
    }
    void prepare_render_frame(CE::RenderAPIs::RenderFrameWriter&) const override {}
    void deinit() override {}
};

int main(int argc, char** argv) {
    auto startup = CE::Engine::make_glfw_opengl_startup("My game", {.title = "My game"});
    auto result = startup.initialize(argc, argv);
    if (!result.should_start())
        return result.exit_code;
    Game game(*result.engine);
    auto runtime = result.make_runtime(game);
    runtime.run();
    return result.exit_code;
}
```

Link this example to `Cheryl::OpenGL::Startup`. Pass `--concurrent` to select the
simulation worker, or use the common timing/polling options above. Applications
that construct `GameRuntime` directly can still pass `RunMode::Concurrent` as its
third argument.
Keep native binding IDs inside bootstrap; gameplay reads semantic `ActionId`s.
Never change bindings or query a live window from concurrent `update()`.

To add presentation, follow [asset loading](../assets/asset-loading.md) and
[pipeline/material construction](../rendering/pipelines-and-materials.md#native-bootstrap-and-binding)
in `init()`, then use CPU submission helpers in frame preparation. For later
replacements, prepare owned data on [workers](../runtime/worker-execution.md),
[submit upload](../runtime/thread-dispatch.md) to platform and adopt only a ready
complete result during update. Keep the prior generation while work is pending.
The [demo implementation](../../projects/apps/demo/src/main.cpp) shows that flow.

Use [input actions and focus](../runtime/input-state-model.md) for controls,
[simulation timing](../runtime/simulation-timing.md) for fixed/variable policies,
and [asset playback](../assets/asset-values-and-playback.md) for per-entity cursors.
The [Unicode service](../assets/text-layout.md) prepares retained text; UI adapters
keep their toolkit authoring APIs. [Audio](../runtime/audio.md) is separately
application-owned and needs its producers settled before closure. World storage,
entities, collision and mechanics belong to the game or its selected modules.

## Dependencies and headers

| Requirement | Target scope and reason |
| --- | --- |
| C++23, Threads, logging configuration | Engine public requirements used by templates/runtime consumers. |
| spdlog, CTTI, GLM | Engine public requirements; legacy `glm.hpp` spelling remains supported. |
| Backward::Interface | Engine public trace/resolver requirements; its own global signal-handler object is not linked. |
| STB and JSON | Engine private implementation include directories. |
| FreeType, HarfBuzz, ICU uc/i18n | Engine private font/layout implementation and link requirements; no library-native types appear in public headers. |
| CLI11 | Startup support's public parsing API; reuse `CLI11::CLI11` or initialize the pinned `extern/cli11` source. Plain Engine/OpenGL consumers do not inherit CLI11 headers or linkage. |
| GLFW | Native GLFW private implementation requirement, also used privately by the OpenGL context binding. |
| Gainput | Native GLFW public requirement when `CHERYL_NATIVE_INPUT` is enabled; its types occur in that owner's headers. |
| X11 | Native input's Linux dependency; OpenGL acceptance also uses it for explicitly selected X11 scenarios. |
| GLAD and OpenGL::GL | OpenGL public generated types and private system link requirement. |
| GoogleTest | Test-only; disabled owners do not discover their integration SDKs. |
| miniaudio | Optional audio module's private implementation/link requirement; public Engine and module audio headers expose no SDK types. |

The Engine umbrellas and granular contracts are neutral. Concrete window/input
headers belong to Native GLFW; `backends/opengl` headers belong to OpenGL. Existing
`cheryl/...` and legacy `core/...`/`assets/...` include spellings are preserved through
each target's public roots. Installed/exported `find_package` distribution remains
separate work.

## Crash and exception traces

`Cheryl::Engine` delivers the automatic crash bootstrap.
Final consumers receive its object directly, so a static linker cannot omit an
unreferenced archive initializer. Installation follows the bootstrap object's
`NDEBUG` scope in the engine build: Debug-style builds install
`backward::SignalHandling`; `NDEBUG` builds do not.
Exception and explicit stack capture remain available through the existing bounded
capture/fallback implementation in all builds. The legacy global trace resolver and
`Cheryl::SignalHandlers` target remain available; the demo needs only the selected
engine/module targets. `NDEBUG` controls signal bootstrap installation, not explicit
trace capture. The legacy `ST_ON_SIGNALS` definition exposes the `sh` declaration
through `core/logging.h`; it does not change installation scope. See the
[signal procedure](architecture-validation.md#manual-acceptance-drivers).

## Independent consumers and header probes

[Engine consumer](../../projects/engine/tests/consumer/CMakeLists.txt) links only
Engine and exercises multiple translation units, events, workers and CPU resources.
Its first-include probes include the neutral umbrellas and clipping contract and reject GL/GLFW
header leakage. Standalone bootstrapping selects Engine only in a local scope.

[Native GLFW consumer](../../projects/modules/platform/native-glfw/tests/consumer/CMakeLists.txt)
and [OpenGL consumer](../../projects/modules/graphics/opengl/tests/consumer/CMakeLists.txt)
link their actual module with first-include probes. They
reference real implementation symbols without requiring a display at execution.
`CHERYL_BUILD_CONSUMER_TESTS=ON` adds consumers for the selected root assembly.

The Engine-only consumer entry point is:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S projects/engine/tests/consumer -B build-consumer-engine -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCHERYL_REPOSITORY_ROOT="$PWD"
  cmake --build build-consumer-engine --target consumer-cengine --parallel
  ./build-consumer-engine/cheryl-consumer
)
```

Standalone consumer entry points require
`CHERYL_REPOSITORY_ROOT=/path/to/cheryl-engine` for shared helpers and bootstrapping.
Engine, Native GLFW and OpenGL consumer bootstraps suppress both UI selections in
a local variable scope, including inherited `ON` choices, without changing the
enclosing configuration's cache.
Normal Native GLFW consumption selects no Cheryl OpenGL/GLAD dependency; the
OpenGL consumer selects both integration owners. CMake 4 hosts may need
`CMAKE_POLICY_VERSION_MINIMUM=3.5` for pinned legacy dependency projects. Standalone
module entry points and explicit null-platform selection are documented separately
in the module guide. [Architecture validation](architecture-validation.md) describes
independent graph, consumer and runner checks and their coverage limits.

## Standard headers in an existing build

An existing CMake/CLion profile can retain failed C++ compiler ABI discovery. Check
whether generated `CMakeCXXCompiler.cmake` records
`CMAKE_CXX_ABI_COMPILED FALSE` and empty implicit include directories. This can turn
the X11 dependency's `/usr/include` into an explicit `-isystem` argument;
GCC can then fail to resolve `math.h` or `stdlib.h` through `#include_next` in
C++ standard headers.

Refresh compiler discovery and regenerate the build files while preserving the
profile/toolchain settings. CMake normally filters its
[detected implicit include directories](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_IMPLICIT_INCLUDE_DIRECTORIES.html)
from explicit compiler arguments. Reload CMake in CLion after repairing an existing
build externally. Preserve intended profile/toolchain/cache settings when resetting
generated discovery metadata or creating a fresh build directory.
