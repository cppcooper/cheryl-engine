# Project setup

Choose how your game obtains Cheryl before writing its entry point. During
development, adding a source checkout to your game's CMake project is sufficient:
building the game also builds the libraries it uses. An installed development SDK
is an alternative when you want to consume already compiled libraries. Both paths
provide the same CMake target names and public `cheryl/` headers.

After setup, continue with [Your first game](first-game.md). It introduces startup,
the game hooks and resource ownership before putting a sprite and text on screen.
The [consumption reference](../development/consuming-engine.md) covers other target
compositions and the full SDK contract.

## Dependencies

Use CMake 3.28 or newer, Ninja, and a C++23 compiler and standard library with
`std::format` support. Install the
[system dependencies](../development/building.md#system-dependencies) required by
your selected modules. The first game uses FreeType, HarfBuzz and ICU for text,
plus Native GLFW, native input and OpenGL for its window, controls and rendering.
It needs a desktop graphics driver supporting an OpenGL 3.3 core context.

For source builds, initialize the checkout's recorded recursive submodules. Python
and Jinja2 are needed when GLAD generates OpenGL bindings. The first game supplies
its own sprite, shaders and manifests; the embedded fallback font comes from the
assets submodule. No optional artwork download is needed.

For an SDK, install its
[consumer prerequisites](../development/consuming-engine.md#prebuilt-development-sdks)
instead of rebuilding Cheryl. Some native development/runtime libraries remain
external on Linux; Windows SDKs include a dependency prefix whose DLLs must be
available at runtime.

## Build your game with a source checkout

A simple layout keeps your game independent of the engine checkout:

```text
projects/
├── cheryl-engine/
└── my-game/
    ├── CMakeLists.txt
    ├── src/
    │   ├── main.cpp
    │   ├── first-game.h
    │   └── first-game.cpp
    └── assets/
```

Clone Cheryl at a revision matching the manual you are reading: `main` for stable,
`develop` for latest, or the matching release tag. Replace `/path/to/projects` with
your chosen parent directory. This block preserves your shell's starting directory:

```sh
(
  set -e
  cd /path/to/projects
  git clone --branch main https://github.com/cppcooper/cheryl-engine.git
  cd cheryl-engine
  git submodule update --init --recursive
)
```

Copy the maintained [first game's](../../projects/apps/first-game/) `src/` and
`assets/` directories into `my-game/` to use its code unchanged. Create
`my-game/CMakeLists.txt` as follows. The options select the first game's
native graphics/input composition and omit unrelated applications and integrations.
The block scopes those settings to Cheryl; it does not force changes into your
project's CMake cache.

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_game LANGUAGES CXX)

set(CHERYL_ENGINE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../cheryl-engine"
    CACHE PATH "Cheryl Engine source checkout")

block(SCOPE_FOR VARIABLES)
    set(CHERYL_BUILD_NATIVE_GLFW ON)
    set(CHERYL_BUILD_OPENGL ON)
    set(CHERYL_NATIVE_INPUT ON)
    set(CHERYL_NATIVE_NULL_PLATFORM OFF)
    set(CHERYL_BUILD_TESTS OFF)
    set(CHERYL_BUILD_CONSUMER_TESTS OFF)
    set(CHERYL_BUILD_DEMO OFF)
    set(CHERYL_BUILD_FIRST_GAME OFF)
    set(CHERYL_BUILD_DOCS OFF)
    set(CHERYL_BUILD_SDK OFF)
    set(CHERYL_BUILD_DEBUG_TERMINAL_LINUX OFF)
    set(CHERYL_BUILD_UI_TGUI OFF)
    set(CHERYL_BUILD_UI_RMLUI OFF)
    set(CHERYL_BUILD_AUDIO_MINIAUDIO OFF)
    set(CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC OFF)
    set(CHERYL_BUILD_WORKER_TOPOLOGY_JSON OFF)
    add_subdirectory("${CHERYL_ENGINE_SOURCE}" cheryl EXCLUDE_FROM_ALL)
endblock()

add_executable(my_game src/main.cpp src/first-game.cpp)
target_link_libraries(my_game PRIVATE Cheryl::Cheryl)
target_compile_definitions(my_game PRIVATE
    CHERYL_FIRST_GAME_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
```

The scoped `set()` values fix this template's component selection; edit them
when adding a module. A same-name `-D` cache option does not override those local
values.

`CHERYL_FIRST_GAME_ASSETS` supplies the example's default asset-root string, so
launching from an IDE or another directory still finds your game's files. As you
replace the example with your own game, keep that explicit connection between
the CMake target and the asset path used by startup.

`Cheryl::Cheryl` provides Engine and the selected modules/support, including their
include directories, feature definitions and dependencies. Your sources can use
`#include <cheryl/cheryl.h>` without collecting individual include paths. Changing
a `CHERYL_BUILD_*` option makes a component available; linking its target is what
provides that component to the application. The facade links the configured set.
Reload CMake in the IDE after changing selections.

Configure and build from the game directory. Replace
`/path/to/projects/my-game` with its location:

```sh
(
  set -e
  cd /path/to/projects/my-game
  cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DGLFW_BUILD_WAYLAND=OFF -DGAINPUT_ENABLE_HID=OFF
  cmake --build build/release --target my_game --parallel
)
```

The policy setting accommodates pinned legacy dependencies on CMake 4 hosts.
The GLFW setting chooses X11 on Linux; the Gainput setting avoids the optional
HID backend and its dependency download. Use the
[build reference](../development/building.md#dependency-and-toolchain-settings)
for other platform choices. No separate engine build or SDK installation is
required. To run the maintained example before writing your own entry point,
follow [First game: setup](first-game.md#setup).

### Choose smaller target sets when needed

The facade is convenient for a game using the configured assembly. Granular
targets let a game select fewer dependencies:

| Link target | What the game obtains |
| --- | --- |
| `Cheryl::Cheryl` | Engine and the configured module/support assembly, including the Linux Debug terminal when selected in a source build. |
| `Cheryl::OpenGL::Startup` | Common command-line startup plus Native GLFW/OpenGL and their Engine requirements; enough for the first game's startup path. |
| `Cheryl::Engine` | Neutral Engine contracts and implementation, without selecting a native backend. |
| `Cheryl::Audio::Miniaudio` | Selected audio integration; add it when your game needs sound. |
| `Cheryl::UI::TGUI` or `Cheryl::UI::RmlUi` | The selected UI integration. |

For example, replace the facade link with
`target_link_libraries(my_game PRIVATE Cheryl::OpenGL::Startup)` for the native
startup composition alone. The [target reference](../../README.md#targets) lists
additional support targets. Header umbrellas expose only components supplied by
the linked targets, so a narrower link can make a feature's conditional include
inactive even when its build option is enabled.

## Use an installed SDK instead

Choose an SDK containing Native GLFW, OpenGL, native input and Startup for the
first game. Its package must match your OS, architecture, compiler/standard-library
ABI and configuration. With an extracted SDK, replace the source-checkout block
in the game CMake file with:

```cmake
find_package(Cheryl 4 CONFIG REQUIRED)
```

Keep `add_executable`, `target_link_libraries` and your asset-root definition.
Configure with the extracted SDK prefix; replace both paths below:

```sh
(
  set -e
  cd /path/to/projects/my-game
  cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/absolute/path/to/extracted-sdk
  cmake --build build/release --target my_game --parallel
)
```

This builds your game, using the libraries already in the SDK. Keep the SDK's
headers, libraries, CMake files and dependencies together. On Windows, also make
the bundled dependency DLL directories available to the process; see the
[SDK consumption contract](../development/consuming-engine.md#prebuilt-development-sdks).
The Linux Debug terminal currently requires source composition and is not exported
in SDKs.

## Build the maintained example independently

The first game's own CMake entry point also works outside the engine's top-level
build, without copying its files or writing another CMake project. From anywhere
inside the checkout, configure against engine source:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S projects/apps/first-game -B build/first-game-source -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCHERYL_FIRST_GAME_ENGINE_SOURCE="$PWD" \
    -DGLFW_BUILD_WAYLAND=OFF -DGAINPUT_ENABLE_HID=OFF
  cmake --build build/first-game-source --target first-game --parallel
  ./build/first-game-source/first-game
)
```

Alternatively, leave the source option empty and select a compatible SDK
containing Native GLFW, native input, OpenGL, and Startup:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S projects/apps/first-game -B build/first-game-sdk -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCHERYL_FIRST_GAME_ENGINE_SOURCE= \
    -DCMAKE_PREFIX_PATH=/absolute/path/to/extracted-sdk
  cmake --build build/first-game-sdk --target first-game --parallel
  ./build/first-game-sdk/first-game
)
```

The example's compiled asset path points to its source `assets` directory. Use
`--assets` when relocating those files with a packaged game. Windows executables
have an `.exe` suffix.

## Produce engine artifacts

The SDK is optional for source consumers. When producing engine artifacts for
others, choose the corresponding operation rather than building every target:

| Need | Operation | Result |
| --- | --- | --- |
| Run the maintained tutorial | Select `CHERYL_BUILD_FIRST_GAME=ON`; build `first-game`. | The tutorial executable and its linked libraries. |
| Build a library directly | Build `cengine`, `module_opengl` or another concrete library target. | The selected library and dependencies in the build tree. |
| Build selected SDK libraries | Select `CHERYL_BUILD_SDK=ON`; build `cheryl-sdk`. | Compiled installable libraries; headers have not yet been collected. |
| Collect libraries and headers | After that build, install component `CherylSDK` to a prefix. | One SDK tree with headers, libraries, CMake imports and licenses. |
| Create a distributable SDK archive | Run `.github/ci/package.py` after the SDK build. | Archive, manifest and checksums; Windows also bundles its native dependencies. |
| Build the website and API reference | Build `docs` in the documentation-only project. | Generated HTML, without compiling Engine. |

Use the [artifact commands](../development/building.md#choose-the-artifact) for
exact configuration, build, install and packaging blocks. `Cheryl::Cheryl` is an
interface joining targets; it does not produce one combined binary.

### Keep headers and binaries matched

A build's selections determine which module libraries, support libraries and
dependencies exist. Configuration also affects compiled logging and Debug
instrumentation. Keep separate build/prefix directories for different configurations
and module selections, and compile game translation units against the headers and
target settings that belong to those libraries. For a packaged SDK, check its
`SDK-MANIFEST.json` for the producer's source/submodule revisions, features,
toolchain and configuration. Matching filenames alone do not establish compatibility.

Engine, Startup and the current compiled module implementations explicitly build
as static libraries. Setting `BUILD_SHARED_LIBS=ON` does not convert these targets
to shared libraries. Some third-party/native dependencies have their own linkage
choices. A shared-library distribution and its binary compatibility policy require
a separate design decision; use the current source-target or matched-SDK contract
for this walkthrough.
