# Cheryl Engine

Cheryl is a C++23 engine with a shared engine library and optional platform,
graphics and UI modules. It supports sequential or concurrent simulation,
retained 2D rendering, input actions/events/text, and CPU asset preparation
separate from graphics resource upload.

The default assembly contains Native GLFW, OpenGL and TGUI, with a windowed demo.
Applications can select the engine alone or link the modules they need.

## Contents

- [Build and run](#build-and-run)
- [CMake options](#cmake-options)
- [Dependencies](#dependencies)
- [Modules and application integration](#modules-and-application-integration)
- [Tests](#tests)
- [Project layout](#project-layout)
- [Troubleshooting](#troubleshooting)
- [Documentation and development](#documentation-and-development)

## Build and run

You need Git, CMake 3.28 or newer, and a C++23 compiler and standard library with
`std::format` support. CMake also needs a build tool such as Ninja or Make.
Install the [system dependencies](#system-dependencies) for the selected modules.

From your checkout, initialize the pinned dependencies and build the demo:

```sh
git submodule update --init --recursive
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target demo --parallel 1
./build/release/demo
```

Building an explicit target compiles its dependencies without building every test
runner. Increase `--parallel` if desired; one job keeps CPU use modest. The commands
use a single-configuration generator; multi-configuration generators also need
`--config Release` when building.

The demo uses a system font and the checked-in shaders without needing the full
image asset tree. WASD pans the camera, R resets it and F5 reloads its shader.
F2 toggles TGUI text focus and F3 hides/shows its panel. Escape releases focus;
Q quits while gameplay has keyboard focus. Closing the window also exits.
Selecting `CHERYL_BUILD_UI_RMLUI=ON` adds an independent RmlUi view, with F4 for
text focus and F6 for visibility. See its [acceptance procedure](projects/modules/ui/rmlui/README.md#acceptance-procedure)
for the focused checks; its executable/native acceptance is still pending.

```sh
./build/release/demo --concurrent
./build/release/demo --fixed --max-updates=120
./build/release/demo --concurrent /path/to/assets
```

`--full-assets` additionally loads the manifest/image tree. Its PNG files are not
tracked and must be supplied under the selected asset root. The default root is
the checkout's `assets/` directory. See the [demo guide](projects/apps/demo/README.md)
for all controls, timing/input options and UI interaction checks.

### Build the engine alone

This selection omits GLFW, Gainput, OpenGL, GLAD, both UI toolkits and FreeType discovery:

```sh
cmake -S . -B build/engine-only -DCMAKE_BUILD_TYPE=Release \
  -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
  -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
  -DCHERYL_BUILD_DEMO=OFF \
  -DCHERYL_BUILD_TESTS=OFF
cmake --build build/engine-only --target cengine --parallel 1
```

`cengine` is the CMake build target for the neutral engine library, whose archive
uses the output name `cheryl-engine`.
Applications use its alias, `Cheryl::Engine`.

## CMake options

Pass settings as `-DNAME=value` during configuration. Defaults below apply to a
fresh root configuration; an existing build or CLion profile retains its cache.
Reload CMake after changing module selection to expose the selected targets.

| Option | Default | Effect |
| --- | --- | --- |
| `CHERYL_BUILD_NATIVE_GLFW` | `ON` | Selects the GLFW display/window module and its optional input implementation. |
| `CHERYL_BUILD_OPENGL` | `ON` | Selects the complete OpenGL backend; requires Native GLFW. |
| `CHERYL_BUILD_UI_TGUI` | `ON` | Selects the TGUI adapter independently of native/graphics modules. |
| `CHERYL_BUILD_UI_RMLUI` | `OFF` | Selects the independent RmlUi adapter; can coexist with TGUI. |
| `CHERYL_BUILD_DEMO` | `ON` | Adds `demo` when OpenGL and native input are selected. |
| `CHERYL_BUILD_TESTS` | `ON` | Adds focused tests and explicitly buildable acceptance/aggregate runners. |
| `CHERYL_BUILD_ALL_TESTS` | `OFF` | Includes owner aggregates and `all-tests` in the default build and CTest discovery. Requires tests. |
| `CHERYL_BUILD_ACCEPTANCE_TESTS` | `OFF` | Includes broader acceptance runners in the default build. Requires tests. |
| `CHERYL_BUILD_CONSUMER_TESTS` | `OFF` | Adds selected owners' independent consumers and first-include header checks. |
| `CHERYL_NATIVE_INPUT` | `ON` | Includes the Gainput-backed input adapter in Native GLFW. |
| `CHERYL_NATIVE_NULL_PLATFORM` | `OFF` | Configures an owned GLFW dependency without X11/Wayland. Pair with native input disabled to omit Gainput/native X11 requirements. |
| `CHERYL_SANDBOX_BUILD` | `OFF` | Deprecated shorthand that defaults native input off and the null platform on. Prefer the explicit native options. |
| `CHERYL_LOG_PROFILE` | `auto` | Selects `developer`, `support` or `release`; `auto` follows the build configuration. |
| `CHERYL_LOG_COMPILED_MASK` | Empty | Overrides compiled severity bits, from `0` to `63`; see [logging policy](docs/runtime/logging.md#compile-policy). |
| `WARN` | `OFF` | Adds the existing `-Wall` compiler flag. |

Native options exist when Native GLFW is selected. The null-platform option
configures GLFW only when Cheryl creates that dependency; a supplied GLFW target
keeps its host's settings. Null-platform OpenGL still needs OpenGL development/link
dependencies, and that selection does not provide the windowed demo.

Useful dependency and toolchain settings:

| Setting | When to use it |
| --- | --- |
| `CMAKE_BUILD_TYPE=Release` or `Debug` | Selects a single-configuration build. Non-MSVC Debug currently enables AddressSanitizer and UndefinedBehaviorSanitizer. |
| `GLFW_BUILD_WAYLAND=OFF` | Selects an X11-only bundled GLFW build on Linux and avoids Wayland development requirements. |
| `GAINPUT_ENABLE_HID=OFF` | Omits the selected Gainput fork's HID support and hidapi fetch. |
| `Python_EXECUTABLE=/path/to/python` | Selects the GLAD generator's interpreter, which must have Jinja2 installed. |
| `CMAKE_POLICY_VERSION_MINIMUM=3.5` | Allows pinned legacy dependency CMake files to configure with CMake 4 when needed. |
| `CHERYL_ENGINE_SOURCE=/path/to/cheryl-engine` | Supplies the engine checkout to a standalone module. |
| `CHERYL_NATIVE_GLFW_SOURCE=/path/to/native-glfw` | Supplies Native GLFW to a standalone OpenGL module. |
| `CHERYL_TGUI_SOURCE=/path/to/TGUI-1.13.0` | Selects a TGUI source tree instead of the bundled submodule. |
| `TGUI_DIR=/path/to/TGUI/cmake/package` | Selects an exact TGUI 1.13.0 package with the required custom/FreeType features. |
| `CHERYL_RMLUI_SOURCE=/path/to/RmlUi-6.3` | Selects a RmlUi source tree instead of the bundled submodule. |
| `RmlUi_DIR=/path/to/RmlUi/cmake/package` | Selects an exact RmlUi 6.3 package with the stock FreeType font engine. |
| `CHERYL_RMLUI_TEST_FONT=/path/to/font.ttf` | Supplies a real font fixture for RmlUi checks when the selected SDK's sample font is unavailable. |

## Dependencies

### Source dependencies

The `extern/` entries below are pinned Git submodules. Their URLs are recorded in
[.gitmodules](.gitmodules); initialize the recorded revisions rather than replacing
them with arbitrary upstream branches. Modules reuse suitable supplied targets
where supported by their [composition contract](docs/development/modules.md).

| Dependency | Selected source | Owner / purpose |
| --- | --- | --- |
| CTTI | [RexarX/ctti](https://github.com/RexarX/ctti), `extern/ctti` | Engine type IDs and names; uses the 1.1 API. |
| GLM | [g-truc/glm](https://github.com/g-truc/glm), `extern/glm` | Engine vectors, matrices and geometry. |
| spdlog | [gabime/spdlog](https://github.com/gabime/spdlog), `extern/spdlog` | Engine logging, configured to use `std::format`. |
| Backward | [bombela/backward-cpp](https://github.com/bombela/backward-cpp), `extern/backward-cpp` | Engine exception traces and Debug crash handling. |
| STB | [nothings/stb](https://github.com/nothings/stb), `extern/stb` | Engine image decoding and font rasterization. |
| JSON | [nlohmann/json](https://github.com/nlohmann/json), `extern/json` | Engine asset-manifest parsing. |
| GLFW | [glfw/glfw](https://github.com/glfw/glfw), `extern/glfw` | Native platform windows/events; OpenGL context binding. |
| Gainput | [jochumdev/gainput](https://github.com/jochumdev/gainput), `extern/gainput` | Native input devices and mappings when input is enabled. |
| GLAD | [Dav1dde/glad](https://github.com/Dav1dde/glad), `extern/glad` | OpenGL entry-point generation using the pinned specification. |
| TGUI | [texus/TGUI](https://github.com/texus/TGUI), `extern/tgui` | Optional TGUI 1.13.0 custom backend and toolkit widgets. |
| RmlUi | [mikke89/RmlUi](https://github.com/mikke89/RmlUi), `extern/rmlui` | Optional RmlUi 6.3 Core with native RML/RCSS authoring. |
| GoogleTest | [google/googletest](https://github.com/google/googletest), `extern/googletest` | Test runners; discovered when tests are enabled. |
| hidapi | [libusb/hidapi](https://github.com/libusb/hidapi), fetched by Gainput | HID controller support. Gainput fetches `hidapi-0.15.0` during configuration when enabled. |

The first HID-enabled configuration needs network access or a prepared
FetchContent cache for hidapi. UI toolkit source/package selection performs no download.

### System dependencies

Package names vary by operating system. Install development headers and link
libraries, in addition to runtime libraries, for the selected owners.

| Requirement | Selection | Notes |
| --- | --- | --- |
| Platform threads | Engine | Discovered through `Threads::Threads`. |
| X11 and extensions | Native Linux input / GLFW X11 | GLFW checks XRandR, Xinerama, XKB, Xcursor, XInput and X Shape headers. Native Gainput input also requires X11. |
| Wayland, xkbcommon and `wayland-scanner` | Bundled GLFW Wayland | Linux GLFW enables X11 and Wayland by default. Set `GLFW_BUILD_WAYLAND=OFF` for an X11-only build. |
| OpenGL | OpenGL module | System headers/link libraries; running the demo also requires a usable graphics driver and display. |
| Python and Jinja2 | GLAD generation | Jinja2 must be available in CMake's selected Python interpreter. |
| FreeType | TGUI or RmlUi module | Toolkit font rasterization. |
| libudev / libusb | Linux hidapi | Development dependencies of the HID backends selected by Gainput's fetched hidapi. |
| libdw, libbfd, or libdwarf/libelf | Backward, optional | Improve source/symbol resolution; availability determines the selected resolver. |
| A discoverable system font | Demo | The HUD uses system-font discovery; TGUI uses its embedded default font. |

Engine-only builds omit the platform, graphics and toolkit requirements above.
The first native acceptance platform is Linux/GLFW/X11/OpenGL; broader Wayland,
OS and device coverage remains separate from a successful build.

## Modules and application integration

Cheryl modules are ordinary CMake libraries, each with its own `include/`, `src/`
and `tests/`. Link targets to inherit headers and dependencies.

| Public target | Build target | Owner |
| --- | --- | --- |
| `Cheryl::Engine` | `cengine` | [projects/engine](projects/engine) |
| `Cheryl::NativeGLFW` | `module_native_glfw` | [Native GLFW](projects/modules/platform/native-glfw/README.md) |
| `Cheryl::OpenGL` | `module_opengl` | [OpenGL](projects/modules/graphics/opengl/README.md) |
| `Cheryl::UI::TGUI` | `module_ui_tgui` | [TGUI](projects/modules/ui/tgui/README.md) |
| `Cheryl::UI::RmlUi` | `module_ui_rmlui` | [RmlUi](projects/modules/ui/rmlui/README.md) |

For an engine-only application in an enclosing CMake project:

```cmake
set(CHERYL_BUILD_TESTS OFF)
set(CHERYL_BUILD_DEMO OFF)
set(CHERYL_BUILD_NATIVE_GLFW OFF)
set(CHERYL_BUILD_OPENGL OFF)
set(CHERYL_BUILD_UI_TGUI OFF)
set(CHERYL_BUILD_UI_RMLUI OFF)
add_subdirectory(path/to/cheryl-engine cheryl)

add_executable(game main.cpp)
target_link_libraries(game PRIVATE Cheryl::Engine)
```

For native graphics, enable Native GLFW and OpenGL and link
`Cheryl::Engine Cheryl::NativeGLFW Cheryl::OpenGL`. Enable and add
`Cheryl::UI::TGUI` or `Cheryl::UI::RmlUi` for toolkit UI, or select both.
Engine never depends on those integrations.
The OpenGL implementation, context and resources stay together in one module.

Build-tree composition is supported. Installed/exported `find_package(Cheryl)`
packaging remains future work. See [consuming the engine](docs/development/consuming-engine.md)
and [standalone modules](docs/development/modules.md#standalone-modules).

## Tests

Tests belong to the owner whose behavior they exercise. Engine unit tests use
controlled contract implementations; native and toolkit implementations have
their own suites. Build the runner you need, then execute it directly:

```sh
cmake --build build/release --target engine-tests ui-tgui-tests --parallel 1
./build/release/tests-engine
./build/release/tests-ui-tgui
```

Build commands and CTest prefixes use target names; executables use the names in
[CherylOutputs.cmake](cmake/CherylOutputs.cmake). For example, `engine-tests`
produces `tests-engine`, and `all-tests` produces `tests-all`.

| Runner / setting | Coverage |
| --- | --- |
| `engine-tests`, `logging-tests` | Focused neutral Engine and logging checks. |
| `native-glfw-tests`, `opengl-tests`, `ui-tgui-tests`, `ui-rmlui-tests` | Selected module implementation checks. |
| `engine-acceptance`, `opengl-acceptance` | Broader runtime, failure, resource and native graphics cases. |
| `engine-all`, `native-glfw-all`, `opengl-all`, `ui-tgui-all`, `ui-rmlui-all` | Each owner's complete GoogleTest runner. |
| `ui-coexist-tests` | Assembly-owned focus and retained-lifetime proof when both UI adapters are selected. |
| `all-tests` | All GoogleTests in the selected assembly; explicitly buildable even when `CHERYL_BUILD_ALL_TESTS=OFF`. |
| `CHERYL_BUILD_CONSUMER_TESTS=ON` | Adds `cheryl-consumer` and selected `cheryl-*-consumer` executables with dependent header probes. |
| `cheryl-logging-acceptance`, `cheryl-signal-acceptance` | Separate manual drivers, covered in [architecture validation](docs/development/architecture-validation.md#manual-acceptance-drivers). |

For one combined run:

```sh
cmake --build build/release --target all-tests --parallel 1
./build/release/tests-all
```

`CHERYL_BUILD_ALL_TESTS=ON` also registers aggregate cases with CTest. Choose a
focused or aggregate CTest prefix to avoid running the same cases repeatedly;
for example, `ctest --test-dir build/release -R '^all-tests\.' --output-on-failure`.
Native graphics cases require `CHERYL_NATIVE_GL_TESTS=1` and a usable display;
font fixtures have separate opt-ins. Skipped cases do not establish that coverage.
See the [test selection guide](docs/development/modules.md#test-ownership-and-selection).

## Project layout

```text
projects/
  engine/                       # Neutral library: include/, src/, tests/, support/
  modules/
    platform/native-glfw/       # Display, windows and optional input
    graphics/opengl/            # Entire OpenGL backend and context
    ui/tgui/                    # Toolkit adapter and module tests
    ui/rmlui/                   # Independent native-document adapter and tests
  apps/demo/                    # Native application and optional UI views
  tests/                        # Cross-project checks and all-tests assembly
  dependency-checks/backward-cpp/
cmake/                          # Composition, test and consumer helpers
extern/                         # Pinned source dependencies
assets/                         # Manifests, shaders and small UI proof images
docs/                           # Current contracts, guides and plans
```

## Troubleshooting

| Symptom | Next step |
| --- | --- |
| A selected dependency directory is empty | Run `git submodule update --init --recursive`. |
| A UI target is missing in CLion | Set its cached `CHERYL_BUILD_UI_TGUI` or `CHERYL_BUILD_UI_RMLUI` option to `ON` and reload CMake. |
| Python reports missing `jinja2` | Install Jinja2 in the interpreter selected by CMake, or set `Python_EXECUTABLE` to one which has it. |
| GLFW reports missing Wayland tools/libraries | Install the Wayland development requirements or configure with `GLFW_BUILD_WAYLAND=OFF`. |
| CMake 4 rejects an old dependency policy version | Configure with `CMAKE_POLICY_VERSION_MINIMUM=3.5`. |
| C++ standard headers cannot find `math.h` or `stdlib.h` in an existing profile | Check stale compiler discovery before editing includes; see [compiler-discovery recovery](docs/development/consuming-engine.md#standard-headers-in-an-existing-build). |
| Touchpad movement pauses while typing | Check the desktop's "Disable while typing" setting; the [demo guide](projects/apps/demo/README.md#interaction-checks) covers simultaneous input checks. |

## Documentation and development

- [Documentation index](docs/README.md): runtime, assets, rendering and resources.
- [Runtime architecture](docs/runtime/runtime-architecture.md): ownership and application/backend boundaries.
- [Module guide](docs/development/modules.md): selection, composition and test ownership.
- [Code style](docs/development/code-style.md) and [agent instructions](AGENTS.md): repository conventions.
- [Development roadmap](docs/planning/develop-review-and-development-plan.md) and
  [unfinished work](docs/planning/todo.md): remaining work and prerequisites.
