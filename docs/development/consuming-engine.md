# Consuming the engine

Start with [project setup](../getting-started/project-setup.md) to choose source
or SDK consumption, then follow the [first-game walkthrough](../getting-started/first-game.md)
from startup and resource ownership through sprite/text submission. This page
provides the detailed source and SDK consumption contracts.

Applications can compose source targets from a checkout or link a prebuilt
[development SDK](#prebuilt-development-sdks). Link `Cheryl::Cheryl` to use the
complete configured assembly through one public include:

```cpp
#include <cheryl/cheryl.h>
```

That target combines Engine, configured modules and available support libraries.
Its include directories and feature definitions make the matching headers visible
to the compiler and the IDE. The same include works with source and SDK targets.
Individual targets remain available when the application needs a smaller dependency
set; `Cheryl::Engine` is the neutral library backed by build target `cengine`.
Its artifact uses the Engine component version; CMake selects the matching filename.

## Source checkout

A relative path to the Cheryl checkout through `add_subdirectory` is supported.
For example, with the root's default module and worker-support selections:

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_game LANGUAGES CXX)
set(CHERYL_BUILD_TESTS OFF)
set(CHERYL_BUILD_DEMO OFF)
set(CHERYL_BUILD_SDK OFF)
set(CHERYL_BUILD_DEBUG_TERMINAL_LINUX OFF)
add_subdirectory(path/to/cheryl-engine cheryl)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE Cheryl::Cheryl)
```

Replace `path/to/cheryl-engine` with the checkout path relative to your application's
`CMakeLists.txt`, or an absolute path. CMake builds the selected libraries and
propagates their public include paths and dependency settings. Reload the CMake
project in the IDE after changing selections so completion uses those settings.
Keep `CHERYL_BUILD_SDK=OFF` for source application builds; aggregate headers and IDE
completion do not require SDK export or installation. Enable it only when also
producing a separate SDK. The [SDK build](building.md#development-sdk-builds)
collects headers and compiled libraries into a separate prefix for consumers that
want a prebuilt distribution.

The source facade also links the configured Linux Debug terminal and its entry
attachment when that module is enabled. The example disables it; use the granular
targets below when you want to choose support and diagnostic behavior individually.

### Windows source-build runtime

Shared libraries must be available when the game starts. With
`add_subdirectory(... cheryl)` and a single-configuration Ninja build in
`build/release`, Cheryl writes its DLLs to `build/release/cheryl`; the game's
executable can be in the parent directory. Multi-configuration generators add a
configuration subdirectory, such as `cheryl/Release`.

After creating and linking `my_game`, copy CMake-known runtime DLLs beside it:

```cmake
get_target_property(cheryl_engine_type Cheryl::Engine TYPE)
if(WIN32 AND cheryl_engine_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET my_game POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy -t "$<TARGET_FILE_DIR:my_game>"
            "$<TARGET_RUNTIME_DLLS:my_game>"
        COMMAND_EXPAND_LISTS
        VERBATIM)
endif()
```

This stages Cheryl and dependency DLLs whose runtime locations are known to CMake.
Native FreeType, HarfBuzz or ICU packages can still require their SDK's DLL
directories on the process `PATH`: CMake's
[`TARGET_RUNTIME_DLLS`](https://cmake.org/cmake/help/v3.28/manual/cmake-generator-expressions.7.html#genex:TARGET_RUNTIME_DLLS)
cannot list DLLs for imported targets without runtime-location metadata.
Alternatively, configure the IDE/launcher `PATH` with both Cheryl's configuration
output directory and the native SDK's DLL directories instead of copying.

### Granular composition

Link selected targets individually to control the application's dependency set.
This example selects neutral Engine alone:

```cmake
set(CHERYL_BUILD_TESTS OFF)
set(CHERYL_BUILD_DEMO OFF)
set(CHERYL_BUILD_NATIVE_GLFW OFF)
set(CHERYL_BUILD_DEBUG_TERMINAL_LINUX OFF)
set(CHERYL_BUILD_OPENGL OFF)
set(CHERYL_BUILD_UI_TGUI OFF)
set(CHERYL_BUILD_UI_RMLUI OFF)
set(CHERYL_BUILD_AUDIO_MINIAUDIO OFF)
set(CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC OFF)
set(CHERYL_BUILD_WORKER_TOPOLOGY_JSON OFF)
set(CHERYL_BUILD_DOCS OFF)
set(CHERYL_BUILD_SDK OFF)
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
The [module guide](modules.md) describes ownership, standalone composition and tests.

## Prebuilt development SDKs

Extract an SDK and select its root through `CMAKE_PREFIX_PATH` or
`Cheryl_DIR=<sdk>/lib/cmake/Cheryl`. No Cheryl implementation checkout is needed
to consume it. Link the same public aliases as source consumers:

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_game LANGUAGES CXX)
find_package(Cheryl 4 CONFIG REQUIRED)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE Cheryl::Cheryl)
# Or link Cheryl::Engine and individual module/support targets for a smaller set.
```

For a Linux Release SDK, configure the application's directory as follows:

```sh
(
  set -e
  cd /path/to/my_game
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/absolute/path/to/extracted-sdk
  cmake --build build --parallel
)
```

The SDK contains compiled Engine, Startup and selected module/topology libraries,
their dependency libraries or headers, the Engine crash bootstrap, public headers,
CMake import/version files and licenses. `.h`/`.hpp` and required `.inl`/`.ipp`/`.tpp`
files remain included. C/C++ implementation translation units, tests, artwork,
applications and the demo are excluded. There is no separate demo package.
Header-based dependencies such as miniaudio still require their public header.
Its `Cheryl::Cheryl` target selects the components distributed in that SDK;
`<cheryl/cheryl.h>` exposes their public interfaces. Installed headers share the
`include/cheryl/` namespace, including the module and support umbrellas described
[below](#dependencies-and-headers).
The [build guide](building.md#development-sdk-builds) describes production;
`SDK-MANIFEST.json` records selected features, source/submodule revisions, OS,
architecture, compiler/version, configuration, logging profile, native versions,
enabled component versions and static/shared linkage.

Use a package compatible with the target OS/distribution, architecture, compiler,
standard-library ABI and configuration. Windows CI SDKs use MSVC x64 RelWithDebInfo
and the DLL CRT; Linux CI SDKs use Ubuntu 24.04 x64 GCC 14 Release. A locally built
SDK's identity describes that local toolchain, not those hosted configurations.
Imported logging definitions are fixed to the producer's compiled profile/mask;
the application must not select contradictory definitions. Match MSVC runtime and
configuration rather than mixing incompatible Debug/Release C++ binaries.

Use Engine and modules from the same matched SDK/binary set. Shared builds give
built-in Engine/Native GLFW singletons, cache/provider state and logging state one
owning library. Application-defined `CE::State::Singleton<T>` and generic allocator
template statics can still have separate instances in different DLLs. Share such
application state through an explicit owner.

Retained object/resource handles keep their captured release contexts; release
through their deleters. For manual destruction, call
`CE::Memory::Objects::Constructor<T>::destroy(...)` in the allocating library, or
retain and use the original construction context. A direct static call from another
DLL can consult a different tracker. Keep the owning libraries loaded while their
objects and deleters remain in use. Plugin hot unloading and arbitrary module-version
mixing are outside this SDK contract.

| Native dependencies | Consumer requirements |
| --- | --- |
| Linux | Install compatible FreeType, HarfBuzz and ICU development/runtime packages; hwloc when selected, the producer's Backward resolver libraries, and X11/OpenGL when selected. Pinned C++ and selected integration dependencies are already supplied. |
| Windows | The `dependencies/` prefix supplies pinned vcpkg headers, import libraries, CMake files, release DLLs and licenses. Keep it with the SDK; shared Cheryl DLLs also live in the SDK's `bin/`. Both DLL directories must be on the application's search path. The MSVC runtime remains a host requirement. |

The package config discovers native libraries, checks the producer's ICU major
version and requires the hwloc 2.5+ (2.x) API when that provider is included.
For an HID-enabled SDK with an owned hidapi dependency, the selected backend,
headers and licenses are packaged. Linux consumers also need pkg-config and the
backend's libudev/libusb development libraries, plus Iconv when selected; the
package config discovers only the dependencies that backend requires. Packaging
does not resolve the [Linux controller limitations](../../projects/modules/platform/native-glfw/README.md#controller-backend-limits).
The package exposes `Cheryl_RUNTIME_DIRECTORIES`: shared SDKs use `lib/` on Linux
and `bin/` on Windows; Windows bundles also include `dependencies/bin/`.
Use those paths for application runtime setup or deploy the required libraries
beside your executable. Linux shared libraries find their packaged neighbors
through an origin-relative runtime path. Windows config also prepends the bundled
dependency prefix for CMake discovery; it does not deploy DLLs automatically.
OpenGL still needs the host's graphics runtime.
SDK relocation does not establish compatibility with another ABI or distribution.

Installed-target consumers and first-include probes cover the selected SDK
composition, including both Startup support headers, on the selected hosted Linux
and Windows toolchains. The [CI guide](building.md#github-ci-and-sdk-artifacts)
retains hosted skipped-case limits and the pending main-push artifact collection.
Desktop, GPU and physical-device coverage remain separate. The SDK does not export
the Linux Debug terminal; that module retains its source-composition path.

## Command-line startup

[`CE::Application::Startup`](../../projects/engine/support/startup/include/cheryl/core/engine/startup.h)
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

Help and parse/configuration errors produce a `CE::Application::StartupResult` with no context.
Return its `exit_code` when `should_start()` is false; help succeeds and invalid
arguments fail before native context construction. A successful result owns the
`CE::Runtime::Context` and the parsed `CE::Application::RuntimeOptions`. Construct the game from that
context, then call `result.make_runtime(game)` to apply its run mode, polling and
timing together. Keep the result and game alive through runtime destruction;
`make_runtime` requires an lvalue result. Backend construction failures propagate,
and `CE::Runtime::GameRuntime::run()` retains the runtime's existing initialization/cleanup contract.

`CE::Application::StartupOptions` supplies application defaults through `execution` and
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

A custom `CE::Application::StartupBackend` provides a required `create(const CE::Runtime::ExecutionOptions&)` factory,
optional Backend argument definitions and optional validation. The factory must
return an owned context; a null result fails startup. Backend selection is currently
programmatic. CLI backend selection, application-preference/build-default resolution
and runtime-loaded discovery remain unimplemented at the selection point after
parsing. [The demo option helper](../../projects/apps/demo/src/demo-options.cpp)
shows application-specific registration without duplicating the engine parser.

## Game hooks and ownership

Implement `CE::Game::AbstractGame` and lend it, with an `CE::Runtime::Context`, to
`CE::Runtime::GameRuntime`. Keep both alive through `CE::Runtime::GameRuntime::run()` returning or throwing. The selected
factory assembles display, window, input, presentation, renderer and resources;
`CE::Runtime::GameRuntime::run()` initializes those adapters before calling the game.

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
#include <cheryl/cheryl.h>
#include <gainput/gainput.h>

class Game final : public CE::Game::AbstractGame {
    CE::Runtime::Context& engine_;
    static constexpr CE::Input::Actions::Id quit_{1};

public:
    explicit Game(CE::Runtime::Context& engine) : engine_(engine) {}
    void init() override {
        auto& input = engine_.input();
        static_cast<void>(input.bindings().bind_button({input.keyboard_id(), gainput::KeyEscape}, quit_));
    }
    void update(const CE::Game::TickContext& tick) override {
        if (tick.input.button(quit_).pressed())
            tick.request_stop();
    }
    void prepare_render_frame(CE::Rendering::FrameWriter&) const override {}
    void deinit() override {}
};

int main(int argc, char** argv) {
    auto startup = CE::Application::GLFWOpenGL::make_startup("My game", {.title = "My game"});
    auto result = startup.initialize(argc, argv);
    if (!result.should_start())
        return result.exit_code;
    Game game(*result.context);
    auto runtime = result.make_runtime(game);
    runtime.run();
    return result.exit_code;
}
```

Link this example to `Cheryl::Cheryl` with OpenGL selected, or to
`Cheryl::OpenGL::Startup` for granular composition. Pass `--concurrent` to select the
simulation worker, or use the common timing/polling options above. Applications
that construct `CE::Runtime::GameRuntime` directly can still pass `RunMode::Concurrent` as its
third argument.
Keep native binding IDs inside bootstrap; gameplay reads semantic `CE::Input::Actions::Id`s.
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

All public Cheryl includes share the `cheryl/` prefix in both source and SDK
consumption. The files remain with their owning projects in the checkout; target
usage requirements present those public roots together to the compiler and IDE.
SDK installation collects the roots into one physical `include/cheryl/` tree.

| Include | Public scope |
| --- | --- |
| `<cheryl/cheryl.h>` | Engine plus the modules and support selected by linked targets. |
| `<cheryl/engine.h>` | All neutral Engine public headers. |
| `<cheryl/modules.h>` | Modules selected by linked targets. |
| `<cheryl/support.h>` | Support libraries selected by linked targets. |
| `<cheryl/modules/opengl.h>` and other `cheryl/modules/` umbrellas | One module's public headers. |
| `cheryl/support/` umbrellas | One support library's public headers. |

The umbrellas list only public Cheryl headers. Some public headers transitively
use `internals/` helpers, so the SDK retains those files as implementation support;
applications do not include them directly. Including an umbrella does not enable
or link a module: CMake target selection supplies its
dependencies and the feature definitions used by the group umbrellas. Link
`Cheryl::Cheryl` for all configured components, or link granular public targets
and use the same umbrellas for that narrower selection. Additional includes such
as `<gainput/gainput.h>` remain appropriate when directly using dependency APIs.

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
| hwloc 2.5+ (2.x) | Optional WorkerTopology::Hwloc private SDK/final link requirement; Engine alone does not discover or link it. |

The Engine umbrellas and granular contracts are neutral. Concrete window/input
headers belong to Native GLFW; `backends/opengl` headers belong to OpenGL. Existing
`cheryl/...` and legacy `core/...`/`assets/...` include spellings are preserved through
each target's public roots, including in the installed SDK.

`<assets.h>` and `<core.h>` expose no Glad types; concrete OpenGL resources are
opt-in through `<backends/opengl.h>`. Portable asset values and primitives live in
`assets/types/`, with shared 2D bases under `assets/types/2d/base/`. CPU geometry
builders live in `assets/geometry/`, parsed grids/clips/manifests in
`assets/definitions/`, and neutral upload/resource interfaces in `assets/resources/`.
`core/rendering/` owns renderer contracts, frame submissions and resolved packets.
Common submission uses public parameter keys; the
[backend mapping](../rendering/pipelines-and-materials.md#native-bootstrap-and-binding)
owns native uniform names and graphics types.

## Crash and exception traces

`Cheryl::Engine` delivers the automatic crash bootstrap. Static builds forward
its object directly to final consumers, so the linker cannot omit an unreferenced
archive initializer. Shared Engine embeds one copy; modules and applications
reuse that copy through Engine. Installation follows the bootstrap object's
`NDEBUG` scope in the engine build: Debug-style builds install
`backward::SignalHandling`; `NDEBUG` builds do not.
Exception and explicit stack capture remain available through the existing bounded
capture/fallback implementation in all builds. Static SDKs also export
`Cheryl::SignalHandlers`; shared SDKs keep the bootstrap inside `Cheryl::Engine`.
The demo needs only the selected engine/module targets. `NDEBUG` controls signal
bootstrap installation, not explicit trace capture. The legacy `ST_ON_SIGNALS`
definition exposes the `sh` declaration through `core/logging.h`; Debug shared
Engine exports its owned instance, and static Debug consumers receive it through
the bootstrap object. The definition does not change installation scope. See the
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

## Optional output terminal

For Linux GNU/Clang consumers, explicitly link `Cheryl::DebugTerminal::Linux` (or
the root's selected `Cheryl::DebugTerminal`) to choose the process entry attachment.
The source `Cheryl::Cheryl` facade also includes it when configured; SDK facades
omit this source-only module and its Debug terminal support.
Plain Engine and command-line Startup do not select it. `AUTO` includes actual
Debug only; `ON` permits a non-Debug consumer to display it with `--debug-terminal`.
See the [module guide](../../projects/modules/platform/debug-terminal-linux/README.md)
for argument consumption, fallback and independent viewer lifetime.

The small `Cheryl::DebugTerminal::Startup` support target accepts an implementation
callback; with no callback it calls the application unchanged. Native capture,
viewer launch and process-wide shutdown live entirely in the selected module.

## Standard headers in an existing build

An existing CMake/CLion profile can retain failed C++ compiler ABI discovery. Check
whether generated `CMakeCXXCompiler.cmake` records
`CMAKE_CXX_ABI_COMPILED FALSE` and empty implicit include directories. This can turn
the X11 dependency's `/usr/include` into an explicit `-isystem` argument;
GCC can then fail to resolve `CE::Math.h` or `stdlib.h` through `#include_next` in
C++ standard headers.

Refresh compiler discovery and regenerate the build files while preserving the
profile/toolchain settings. CMake normally filters its
[detected implicit include directories](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_IMPLICIT_INCLUDE_DIRECTORIES.html)
from explicit compiler arguments. Reload CMake in CLion after repairing an existing
build externally. Preserve intended profile/toolchain/cache settings when resetting
generated discovery metadata or creating a fresh build directory.
