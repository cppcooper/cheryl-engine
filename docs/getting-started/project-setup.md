# Project setup

Choose how your game obtains Cheryl before writing `main()`. During
development, adding a source checkout to your game's CMake project is sufficient:
building the game also builds the libraries it uses. A development **SDK** is a
directory containing compiled libraries, public headers and CMake package files;
use it when you want to link already compiled Cheryl libraries. Both paths
provide the same public CMake names, such as `Cheryl::Cheryl`, for
`target_link_libraries()`, and the same `cheryl/` headers.

After setup, continue with [Your first game](first-game.md). It introduces startup,
the game hooks and resource ownership before putting a sprite and text on screen.
The [consumption reference](../development/consuming-engine.md) covers other
library selections and the full SDK requirements.

## Dependencies

Use CMake 3.28 or newer, Ninja, and a C++23 compiler and standard library with
`std::format` support. Install the
[system dependencies](../development/building.md#system-dependencies) required by
your selected modules. The first game uses FreeType, HarfBuzz and ICU for text,
plus `Cheryl::NativeGLFW` for its window/input and `Cheryl::OpenGL` for rendering.
It needs a desktop graphics driver supporting an OpenGL 3.3 core context.

For source builds, initialize the checkout's recorded recursive submodules. Python
and Jinja2 are needed when GLAD generates OpenGL bindings. The first game supplies
its own sprite image, shaders and `assets/graphics/graphics-manifests.json`, an
index selecting its graphics definitions. The embedded fallback font comes from
the engine checkout's `assets/` submodule. No optional artwork download is needed.

For an SDK, install its
[consumer prerequisites](../development/consuming-engine.md#prebuilt-development-sdks)
instead of rebuilding Cheryl. Some native development/runtime libraries remain
external on Linux; Windows SDKs bundle native libraries under `dependencies/`,
whose `.dll` files must be available when your game runs.

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
`my-game/CMakeLists.txt` as follows. The `CHERYL_BUILD_*` options select the first
game's Native GLFW/OpenGL libraries and omit unrelated applications and integrations.
`block(SCOPE_FOR VARIABLES)` limits those settings to the enclosed Cheryl setup;
it does not replace saved command-line/IDE options in `CMakeCache.txt`.

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

`add_subdirectory()` reads the engine checkout's CMake project and places its
build files under the game's `cheryl/` build subdirectory. `add_executable()`
creates the executable target named `my_game`; `target_link_libraries()` attaches
the selected Cheryl libraries and their compiler settings to it.

The scoped `set()` values fix this template's module selection; edit them
when adding a module. A same-name `-D` cache option does not override those local
values.

`CHERYL_FIRST_GAME_ASSETS` supplies the example's default asset-root string, so
launching from an IDE or another directory still finds your game's files. As you
replace the example with your own game, keep that explicit connection between
the executable target and the asset path read by `main()`.

`Cheryl::Cheryl` is an **interface target**: it links Engine and the selected
module/support libraries and supplies their include directories and preprocessor
definitions. It produces no combined library of its own. Your sources can use
`#include <cheryl/cheryl.h>`: this aggregate header includes Engine and the public
module/support headers selected by those definitions. For example, linking
`Cheryl::OpenGL` supplies `CHERYL_HAS_OPENGL=1`, which enables
`<cheryl/modules/opengl.h>` inside the aggregate header.

A module option such as `CHERYL_BUILD_OPENGL` makes `Cheryl::OpenGL` available;
linking its target supplies that library to your game. `Cheryl::Cheryl` links the
configured set. Reload CMake in the IDE after changing selections.

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

`cmake -S . -B build/release` configures the build. `CMAKE_BUILD_TYPE=Release`
selects the optimization/debug/logging configuration; module options such as
`CHERYL_BUILD_OPENGL` select libraries. `cmake --build ... --target my_game`
then compiles that executable and the libraries it links.

The policy setting accommodates pinned legacy dependencies on CMake 4 hosts.
The GLFW setting chooses X11 on Linux; the Gainput setting avoids the optional
HID backend and its dependency download. Use the
[build reference](../development/building.md#dependency-and-toolchain-settings)
for other platform choices. No separate engine build or SDK installation is
required. To run the maintained example before writing your own entry point,
follow [First game: setup](first-game.md#setup).

### Choose smaller target sets when needed

Link `Cheryl::Cheryl` for all selected libraries, or choose individual public
targets for fewer dependencies:

| Link target | What the game obtains |
| --- | --- |
| `Cheryl::Cheryl` | Engine and the configured module/support libraries, including the Linux Debug terminal when selected in a source build. |
| `Cheryl::OpenGL::Startup` | The `CE::Application::GLFWOpenGL::make_startup(...)` function used by `main()`, plus common startup, Native GLFW/OpenGL and their Engine requirements. |
| `Cheryl::Engine` | Neutral Engine interfaces and implementation, without selecting a window or graphics implementation. |
| `Cheryl::Audio::Miniaudio` | Selected audio integration; add it when your game needs sound. |
| `Cheryl::UI::TGUI` or `Cheryl::UI::RmlUi` | The selected UI integration. |

For example, replace the `Cheryl::Cheryl` link with
`target_link_libraries(my_game PRIVATE Cheryl::OpenGL::Startup)` for the native
startup libraries alone. In source builds, names such as `Cheryl::Engine` are
**aliases** for concrete build targets: link `Cheryl::Engine`, but use
`cmake --build ... --target cengine` to build its library directly. SDKs expose
the same public names as imported targets referring to existing libraries. The
[target reference](../../README.md#targets) maps the other public and build names.

`<cheryl/cheryl.h>` includes only the modules supplied by linked targets, so a
narrower link can make a feature's conditional include inactive even when its
build option is enabled.

## Use an installed SDK instead

Choose an SDK providing `Cheryl::OpenGL::Startup` and Native GLFW with native input
for the first game. Its package must match your OS, CPU architecture,
compiler/standard-library **ABI** (the binary conventions needed to link C++ code)
and build configuration, such as Release. With an extracted SDK, replace the
source-checkout `block(...)` through `endblock()` in the game CMake file with:

```cmake
find_package(Cheryl 4 CONFIG REQUIRED)
```

`find_package()` loads the SDK's `CherylConfig.cmake`, which provides its public
targets and dependency settings. Keep `add_executable`, `target_link_libraries`
and your asset-root definition. The **SDK prefix** is the extracted root directory
containing `include/`, libraries and CMake package files. Set `CMAKE_PREFIX_PATH`
to that directory so `find_package()` can locate it; replace both paths below:

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

The first game's own `projects/apps/first-game/CMakeLists.txt` also works outside
the engine's top-level build, without copying files or writing another CMake
project. `CHERYL_FIRST_GAME_ENGINE_SOURCE` selects an engine checkout for that
project. From anywhere inside the checkout, configure against engine source:

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

Alternatively, leave `CHERYL_FIRST_GAME_ENGINE_SOURCE` empty to use
`find_package(Cheryl)` and select a compatible SDK containing
`Cheryl::OpenGL::Startup` and Native GLFW with native input:

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

The SDK is optional for source consumers. To produce a file or directory for
others, choose its build, install or packaging operation:

| Need | Operation | Result |
| --- | --- | --- |
| Run the maintained tutorial | Select `CHERYL_BUILD_FIRST_GAME=ON`; build `first-game`. | The tutorial executable and its linked libraries. |
| Build a library directly | Build `cengine`, `module_opengl` or another concrete library target. | The selected library and dependencies in the build tree. |
| Build selected SDK libraries | Select `CHERYL_BUILD_SDK=ON`; build custom target `cheryl-sdk`, which builds the selected libraries together. | Compiled installable libraries; headers have not yet been collected. |
| Collect libraries and headers | After that build, use `cmake --install` with `--component CherylSDK` and a destination `--prefix`. | One SDK directory with headers, libraries, CMake package files and licenses. |
| Create a distributable SDK archive | Run `.github/ci/package.py` after the SDK build. | Archive, `SDK-MANIFEST.json` describing its build, and `SHA256SUMS` for archive verification; Windows also bundles its native dependencies. |
| Build the website and API reference | Run `./generate-docs.sh`, or build `docs` in the documentation-only project. | Generated HTML, without compiling Engine. |

Use the [artifact commands](../development/building.md#choose-the-artifact) for
exact configuration, build, install and packaging blocks.

### Keep headers and binaries matched

A build's module selections determine which libraries and dependencies exist.
`CMAKE_BUILD_TYPE` also affects compiled logging and Debug instrumentation. Keep
separate build directories and installed SDK prefixes for different configurations
and module selections. Compile each game `.cpp` file against the headers and
target settings that belong to those libraries. For a packaged SDK, check its
`SDK-MANIFEST.json` for the producer's source/submodule revisions, features,
toolchain and configuration. Matching filenames alone do not establish compatibility.

`Cheryl::Engine`, `Cheryl::Startup` and the current compiled module implementations
build as static libraries, whose code is linked into the executable. Setting
`BUILD_SHARED_LIBS=ON` does not convert these targets to shared libraries loaded
at runtime. Some third-party/native dependencies have their own linkage choices.
A shared-library distribution and its binary compatibility policy require a
separate design decision; use the current source-build or matched-SDK approach
for this walkthrough.
