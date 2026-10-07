# Cheryl Engine

Cheryl is a C++23 engine with a shared engine library and optional platform,
graphics and UI modules. It supports sequential or concurrent simulation,
retained 2D rendering, input actions/events/text, and CPU asset preparation
separate from graphics resource upload.

The default assembly contains Native GLFW, OpenGL, TGUI and RmlUi, with a
windowed demo.
Applications can select the engine alone or link the modules they need.

## Contents

- [Setup](#setup)
- [Targets](#targets)
- [Tests](#tests)
- [Dependencies](#dependencies)
- [Modules and application integration](#modules-and-application-integration)
- [Project layout](#project-layout)
- [Troubleshooting](#troubleshooting)
- [Documentation and development](#documentation-and-development)

## Setup

You need Git, CMake 3.28 or newer, and a C++23 compiler and standard library with
`std::format` support. CMake also needs a build tool such as Ninja or Make.
Install the [system dependencies](#system-dependencies) for the selected modules.

### Clone and initialize submodules

```sh
git clone https://github.com/cppcooper/cheryl-engine.git
cd cheryl-engine
git submodule update --init --recursive
```

### Update submodules

After changing branches or updating the checkout, synchronize URLs and restore
the submodule revisions recorded by that checkout:

```sh
git submodule sync --recursive
git submodule update --init --recursive
```

### CMake configuration

#### CMake options

Pass settings as `-DNAME=value` during configuration. Defaults below apply to a
fresh root configuration; an existing build or CLion profile retains its cache.
Reload CMake after changing module selection to expose the selected targets.

| Option | Default | Effect |
| --- | --- | --- |
| `CHERYL_BUILD_NATIVE_GLFW` | `ON` | Selects the GLFW display/window module and its optional input implementation. |
| `CHERYL_BUILD_OPENGL` | `ON` | Selects the complete OpenGL backend; requires Native GLFW. |
| `CHERYL_BUILD_UI_TGUI` | `ON` | Selects the TGUI adapter independently of native/graphics modules. |
| `CHERYL_BUILD_UI_RMLUI` | `ON` | Selects the independent RmlUi adapter; can coexist with TGUI. |
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
| `CMAKE_BUILD_TYPE=Debug`, `RelWithDebInfo` or `Release` | Selects a single-configuration build. Non-MSVC Debug currently enables AddressSanitizer and UndefinedBehaviorSanitizer. |
| `GLFW_BUILD_WAYLAND=OFF` | Selects an X11-only bundled GLFW build on Linux and avoids Wayland development requirements. |
| `GAINPUT_ENABLE_HID=OFF` | Omits the selected Gainput fork's HID support and hidapi fetch. |
| `Python_EXECUTABLE=/path/to/python` | Selects the GLAD generator's interpreter, which must have Jinja2 installed. |
| `CMAKE_POLICY_VERSION_MINIMUM=3.5` | Allows pinned legacy dependency CMake files to configure with CMake 4 when needed. |
| `CHERYL_REPOSITORY_ROOT=/path/to/cheryl-engine` | Supplies the checkout for helpers and dependency bootstrapping in standalone modules/consumers; the root configuration sets it automatically. |
| `CHERYL_NATIVE_GLFW_SOURCE=/path/to/native-glfw` | Supplies Native GLFW to a standalone OpenGL module. |
| `CHERYL_TGUI_SOURCE=/path/to/TGUI-1.13.0` | Selects a TGUI source tree instead of the bundled submodule. |
| `TGUI_DIR=/path/to/TGUI/cmake/package` | Selects an exact TGUI 1.13.0 package with the required custom/FreeType features. |
| `CHERYL_RMLUI_SOURCE=/path/to/RmlUi-6.3` | Selects a RmlUi source tree instead of the bundled submodule. |
| `RmlUi_DIR=/path/to/RmlUi/cmake/package` | Selects an exact RmlUi 6.3 package with the stock FreeType font engine. |
| `CHERYL_RMLUI_TEST_FONT=/path/to/font.ttf` | Supplies a real font fixture for RmlUi checks when the selected SDK's sample font is unavailable. |

#### Common build configurations

Choose a configuration in its own build directory. These examples use the default
module selection and a single-configuration generator. Debug includes
AddressSanitizer/UndefinedBehaviorSanitizer on non-MSVC compilers and developer
logging:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -G Ninja
```

RelWithDebInfo combines optimization and debug symbols with support logging:

```sh
cmake -S . -B build/support -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja
```

Release uses optimization and release logging:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -G Ninja
```

Build an explicit [target](#targets) to compile its dependencies without building
every runner. The following commands use `build/release`; substitute your chosen
directory. Builds use Ninja's native parallelism at normal priority.
Multi-configuration generators need `--config Release` when building and `-C Release`
with CTest; executables
normally appear in a `Release/` subdirectory. Windows executable names add `.exe`.

### Run all tests

With `CHERYL_BUILD_TESTS=ON`, build and run the combined GoogleTest runner for all
selected owners:

```sh
cmake --build build/release --target all-tests --parallel
./build/release/tests-all
```

The aggregate includes focused and GoogleTest acceptance suites, plus coexistence
checks when both UI adapters are selected. Manual drivers, consumers and dependency
checks have separate [targets and procedures](#tests). Native graphics cases need
`CHERYL_NATIVE_GL_TESTS=1` and a usable display; font fixtures have separate opt-ins.
Skipped cases do not establish that coverage.

If configured with `CHERYL_BUILD_ALL_TESTS=ON`, CTest also discovers the aggregate.
Select its prefix to run the combined cases once:

```sh
ctest --test-dir build/release -R '^all-tests\.' --output-on-failure
```

Use either the direct runner or this CTest selection for a combined run.

### Run the demo

```sh
cmake --build build/release --target demo --parallel
./build/release/demo
```

The demo uses a system font and checked-in shaders without needing the full image
asset tree. WASD pans the camera, R resets it and F5 reloads its shader. F2 toggles
TGUI text focus and F3 hides/shows its panel. F4 toggles RmlUi text focus and F6
hides/shows its view. Escape releases focus; Q quits while gameplay has keyboard
focus. Closing the window also exits. Each UI view follows its module selection.
RmlUi's [acceptance procedure](projects/modules/ui/rmlui/README.md#acceptance-procedure)
provides the focused module, composition and native checks.

```sh
./build/release/demo --concurrent
./build/release/demo --fixed --max-updates=120
./build/release/demo --concurrent /path/to/assets
```

`--full-assets` additionally loads the manifest/image tree. Its PNG files are not
tracked and must be supplied under the selected asset root; the
[asset package catalog](docs/assets/catalog.md) lists download sources and image
placement. The default root is the checkout's `assets/` directory. See the
[demo guide](projects/apps/demo/README.md)
for all controls, timing/input options and UI interaction checks.

### Compile-time options (macros)

Link Cheryl targets to inherit their compile definitions. Logging definitions
must agree across every translation unit sharing Cheryl headers; select the
CMake logging options above for a target-wide policy. Runtime log levels cannot
restore a compiled-out severity. See the
[logging compile policy](docs/runtime/logging.md#compile-policy).

| Macro | Values / default | Effect |
| --- | --- | --- |
| `CHERYL_LOG_PROFILE` | `0` developer, `1` support, `2` release; CMake sets it from `CHERYL_LOG_PROFILE`. | Selects compiled severity defaults and runtime logging presets. Header-only fallback is developer without `NDEBUG`, release with it. |
| `CHERYL_LOG_COMPILED_MASK` | Severity bits `0`–`63`; profile defaults are `0x3f`, `0x1f`, `0x0f`. | Selects compiled severities: fatal `0x01`, error `0x02`, warning `0x04`, info `0x08`, debug `0x10`, trace `0x20`. `0` compiles all out. |
| `CTWriteMask` | Legacy alias/override of `CHERYL_LOG_COMPILED_MASK`. | Must match the compiled mask across shared templates; an include-local override is unsupported. |
| `NDEBUG` | Normally unset in Debug, defined in Release/RelWithDebInfo by the toolchain. | Disables the automatic Backward signal-handler bootstrap; exception/explicit traces remain available. |
| `ST_ON_SIGNALS` | Undefined by default. | Exposes the legacy `backward::SignalHandling sh` declaration through `cheryl/core/logging.h`; installation still follows `NDEBUG`. |

CMake supplies these definitions from selected modules and fixture paths:

| Macro | Source | Effect |
| --- | --- | --- |
| `CHERYL_NATIVE_INPUT` | Native GLFW's option, exported as `0` or `1`. | Selects native input headers/implementation and OpenGL factory overloads. |
| `CHERYL_DEMO_TGUI`, `CHERYL_DEMO_RMLUI` | Defined as `1` when the corresponding UI target exists. | Includes each demo UI view. |
| `CHERYL_SOURCE_DIR` | Repository path on demo and test-support targets. | Supplies the default checkout asset/fixture root. |
| `CHERYL_NATIVE_X11_TESTS` | Defined as `1` for Linux OpenGL acceptance with GLFW X11 selected. | Compiles native X11 acceptance cases. |
| `CHERYL_TEST_FIXTURE_DIR` | OpenGL acceptance fixture directory. | Locates checked-in graphics fixtures. |
| `CHERYL_RMLUI_TEST_FONT` | Explicit CMake setting or the selected SDK's sample font. | Supplies the real font fixture to RmlUi and coexistence checks/consumers. |
| `CHERYL_RMLUI_TEST_IMAGE`, `CHERYL_RMLUI_TEST_BLUE_IMAGE` | RmlUi unit fixture paths. | Supplies image decoding/replacement fixtures. |

## Targets

Use concrete target names with `cmake --build ... --target`. Applications link
public `Cheryl::...` aliases. Target names are declared in
[CherylTargets.cmake](cmake/CherylTargets.cmake); artifact names are declared in
[CherylOutputs.cmake](cmake/CherylOutputs.cmake). Archives and executables use the
configured build root, with platform-specific prefixes/extensions. Interface
libraries convey requirements without a build rule; object libraries have no
standalone archive or executable.

### Core

| Target | Public alias | Kind / output name | Purpose |
| --- | --- | --- | --- |
| `cengine` | `Cheryl::Engine` | Static library: `cheryl-engine` | Neutral engine contracts and implementation. |
| `cengine_logging_config` | — | Interface library | Shared compile-time logging policy. |
| `cengine_signal_handlers` | `Cheryl::SignalHandlers` | Object library | Crash bootstrap for builds without `NDEBUG`, forwarded by Engine. |

### Modules

| Target | Public alias | Output name | Selection / purpose |
| --- | --- | --- | --- |
| `module_native_glfw` | `Cheryl::NativeGLFW` | `cheryl-module-native-glfw` | `CHERYL_BUILD_NATIVE_GLFW`: display/windows and optional input; [owner guide](projects/modules/platform/native-glfw/README.md). |
| `module_opengl` | `Cheryl::OpenGL` | `cheryl-module-opengl` | `CHERYL_BUILD_OPENGL`: graphics, context and resources; [owner guide](projects/modules/graphics/opengl/README.md). |
| `module_ui_tgui` | `Cheryl::UI::TGUI` | `cheryl-module-ui-tgui` | `CHERYL_BUILD_UI_TGUI`: TGUI adapter; [owner guide](projects/modules/ui/tgui/README.md). |
| `module_ui_rmlui` | `Cheryl::UI::RmlUi` | `cheryl-module-ui-rmlui` | `CHERYL_BUILD_UI_RMLUI`: RmlUi adapter; [owner guide](projects/modules/ui/rmlui/README.md). |
| `gl46` | — | `gl46` | OpenGL's generated GLAD support library. |

### Applications

| Target | Executable | Selection |
| --- | --- | --- |
| `demo` | `demo` | `CHERYL_BUILD_DEMO`, OpenGL and native input enabled; [demo guide](projects/apps/demo/README.md). |

### Tests

Tests belong to the owner whose behavior they exercise. Engine tests use controlled
contract implementations; native and toolkit implementations have their own
suites. Test runners require `CHERYL_BUILD_TESTS=ON` and their owner selected.
GoogleTest CTest prefixes use the concrete target name followed by `.`.

#### Unit tests

These focused runners join the default build and CTest discovery:

| Target | Executable | Coverage |
| --- | --- | --- |
| `tests-engine` | `tests-engine` | Focused neutral Engine checks. |
| `tests-logging` | `tests-engine-logging` | Engine logging checks. |
| `tests-native-glfw` | `tests-native-glfw` | Native GLFW diagnostics and optional input mapping. |
| `tests-opengl` | `tests-opengl` | Mock OpenGL checks. |
| `tests-ui-tgui` | `tests-ui-tgui` | TGUI input, recording, session and controlled runtime checks. |
| `tests-ui-rmlui` | `tests-ui-rmlui` | RmlUi input, recording, session and controlled runtime checks. |

#### Aggregate tests

Aggregates are explicitly buildable; `CHERYL_BUILD_ALL_TESTS=ON` also includes them
in the default build and CTest discovery. They reuse the focused/acceptance case
objects, so choose one runner or CTest prefix for overlapping cases.

| Target | Executable | Coverage |
| --- | --- | --- |
| `all-engine` | `tests-all-engine` | Engine unit/acceptance and logging unit cases. |
| `all-native-glfw` | `tests-all-native-glfw` | All selected Native GLFW cases. |
| `all-opengl` | `tests-all-opengl` | OpenGL mock and selected native acceptance cases. |
| `all-ui-tgui` | `tests-all-ui-tgui` | All TGUI cases. |
| `all-ui-rmlui` | `tests-all-ui-rmlui` | All RmlUi cases. |
| `all-tests` | `tests-all` | All selected owners' GoogleTests and assembly coexistence cases. |

#### Acceptance tests

Acceptance targets are explicitly buildable; `CHERYL_BUILD_ACCEPTANCE_TESTS=ON`
also includes them in the default build and discovers the GoogleTest cases.

| Target | Executable | Procedure / coverage |
| --- | --- | --- |
| `acceptance-engine` | `tests-acceptance-engine` | GoogleTest runtime, concurrency, failure and resource cases. |
| `acceptance-opengl` | `tests-acceptance-opengl` | GoogleTest native graphics/runtime cases; requires native input, `CHERYL_NATIVE_GL_TESTS=1` and a usable display. |
| `acceptance-logging` | `tests-acceptance-engine-logging` | Manual timeout/fault/static-teardown driver via `projects/engine/tests/logging-acceptance/logging.py`. |
| `acceptance-signal` | `tests-acceptance-engine-signal` | Manual POSIX Debug/Release crash-bootstrap driver via `projects/engine/tests/signal-acceptance/signals.py`. |

The manual drivers remain outside GoogleTest aggregates and CTest discovery. See
[architecture validation](docs/development/architecture-validation.md#manual-acceptance-drivers)
for their invocation and coverage limits.

#### Cross-module tests

| Target | Executable | Selection / coverage |
| --- | --- | --- |
| `tests-ui-coexist` | `tests-ui-coexist` | Both UI adapters selected; focus preemption and retained lifetimes across toolkits. Also joins `all-tests`. |

#### Dependency checks

| Target | Executable | Coverage |
| --- | --- | --- |
| `backward-cpp` | `tests-dependency-backward-cpp` | Standalone Backward dependency stack-trace check. |

#### Consumer tests

`CHERYL_BUILD_CONSUMER_TESTS=ON` adds the selected owners' consumers independently
of GoogleTest selection. Building a consumer also compiles its first-include
header probes. Independent composition uses each owner's `tests/consumer/` entry
point with `CHERYL_REPOSITORY_ROOT` supplied; see
[consuming the engine](docs/development/consuming-engine.md#independent-consumers-and-header-probes).

| Target | Executable | Owner |
| --- | --- | --- |
| `consumer-cengine` | `cheryl-consumer` | Engine. |
| `consumer-module-native-glfw` | `cheryl-native-glfw-consumer` | Native GLFW. |
| `consumer-module-opengl` | `cheryl-opengl-consumer` | OpenGL. |
| `consumer-module-ui-tgui` | `cheryl-ui-tgui-consumer` | TGUI. |
| `consumer-module-ui-rmlui` | `cheryl-ui-rmlui-consumer` | RmlUi. |

#### Header probes

These object targets compile public headers as the first include and produce no
executable. Their selection follows the corresponding consumer.

| Target | Owner |
| --- | --- |
| `consumer-headers-cengine` | Engine neutral umbrellas and contracts. |
| `consumer-module-headers-native-glfw` | Native GLFW public contracts. |
| `consumer-module-headers--opengl` | OpenGL public contracts. |
| `consumer-module-headers--ui-tgui` | TGUI public contracts. |
| `consumer-module-headers--ui-rmlui` | RmlUi public contracts. |

#### Test support

| Target | Kind | Purpose |
| --- | --- | --- |
| `cheryl_test_support` | Interface library | Engine, GoogleTest, test includes and source-root definitions. |
| `cheryl_test_main` | Interface library | Shared main sources and runner support. |
| `<suite-target>-cases` | Object library | Internal case objects compiled once and linked into focused/aggregate runners. |

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
| FreeType | Engine; also TGUI/RmlUi | Neutral font inspection and grayscale rasterization; toolkit font services remain independent. |
| HarfBuzz and ICU (uc/i18n) | Engine | Unicode shaping, paragraph bidi, grapheme/line boundaries. HarfBuzz uses a supplied/CMake target or pkg-config. |
| libudev / libusb | Linux hidapi | Development dependencies of the HID backends selected by Gainput's fetched hidapi. |
| libdw, libbfd, or libdwarf/libelf | Backward, optional | Improve source/symbol resolution; availability determines the selected resolver. |
| A discoverable system font | Demo | The HUD uses system-font discovery; TGUI uses its embedded default font. |

Engine-only builds omit the platform, graphics and toolkit requirements above.
The first native acceptance platform is Linux/GLFW/X11/OpenGL; broader Wayland,
OS and device coverage remains separate from a successful build.

## Modules and application integration

Cheryl modules are ordinary CMake libraries, each with its own `include/`, `src/`
and `tests/`. Link targets to inherit headers and dependencies.

The [core](#core) and [module](#modules) targets provide public aliases for
application links. An Engine-only root build omits GLFW, Gainput, OpenGL, GLAD,
both UI toolkits; FreeType remains an Engine font dependency:

```sh
cmake -S . -B build/engine-only -DCMAKE_BUILD_TYPE=Release -G Ninja \
  -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
  -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
  -DCHERYL_BUILD_DEMO=OFF -DCHERYL_BUILD_TESTS=OFF
```

```sh
cmake --build build/engine-only --target cengine --parallel
```

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
- [Planning catalogue](docs/planning/README.md): short-, mid- and long-term plans,
  remaining work and prerequisites.
