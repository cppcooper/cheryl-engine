# Cheryl Engine

Cheryl is a C++23 engine with one neutral engine library and optional platform,
graphics, UI and audio modules. It provides sequential or concurrent simulation,
retained 2D rendering, input actions/events/text, Unicode layout and CPU asset
preparation separate from graphics upload. Game state and mechanics belong to the
application.

[Read the documentation](https://cheryl-engine.readthedocs.io/) for the API reference,
setup guides and first-game walkthrough.

The default assembly includes Startup, Workers with hwloc/JSON, Native GLFW,
OpenGL, TGUI, RmlUi and miniaudio. The demo and tests are opt-in.
Single-configuration Linux GNU/Clang Debug builds also select the optional
output-terminal module by default. Other configurations default it to `OFF`.

## Contents

- [Online documentation](https://cheryl-engine.readthedocs.io/)
- [Setup](#setup)
- [Targets](#targets)
- [Tests](#tests)
- [CI and development SDKs](#ci-and-development-sdks)
- [Repository architecture](#repository-architecture)
- [Documentation and development](#documentation-and-development)

## Setup

You need Git, CMake 3.28 or newer, Ninja, and a C++23 compiler and standard library
with `std::format` support. The default root configuration also requires Python
3.11 or newer for documentation targets and hwloc 2.5+ (2.x) for worker support.
Set `CHERYL_BUILD_DOCS=OFF` or `CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=OFF` to omit
the corresponding requirement. Install the selected modules'
[system dependencies](docs/site/development/building.md#system-dependencies).
Linux builds use ordinary shared development packages. Windows requires Windows
10 version 1903+ and the SDK's ICU C API; prepare its external shared dependencies
using the [dependency helper](docs/site/development/building.md#dependency-and-toolchain-settings).

```sh
git clone https://github.com/cppcooper/cheryl-engine.git
cd cheryl-engine
git submodule update --init --recursive
```

Configure and build the demo from anywhere inside the checkout:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCHERYL_BUILD_DEMO=ON
  cmake --build build/release --target demo --parallel
  ./build/release/demo
)
```

The command uses Linux system packages. Windows uses the prepared
`x64-windows` prefix and DLL CRT configuration from the build guide. FreeType,
HarfBuzz, GLFW, the UI toolkits and optional hwloc/HIDAPI are shared dependencies.
Windows uses built-in ICU; Linux uses its distribution's ICU. miniaudio, spdlog,
Gainput and GLAD remain embedded in their Cheryl owners.

The demo starts with checked-in shaders and an embedded font. Optional package
images add tile/sprite samples; missing images do not prevent startup. WASD pans,
R resets the camera, F2 selects text focus, Escape releases it and Q quits.
Use `--concurrent` for a simulation worker. The
[demo guide](projects/apps/demo/README.md) covers all controls, artwork and runtime
options.

After changing branches or updating the checkout, restore its pinned dependencies:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  git submodule sync --recursive
  git submodule update --init --recursive
)
```

The [build guide](docs/site/development/building.md) lists configuration options,
dependency settings, Debug/Release behavior and common setup failures. Existing
build directories retain cached options; reload CMake after changing selections.

## Targets

Build concrete target names with `cmake --build ... --target`; applications link
the public aliases to inherit headers and dependencies.

| Build target | Public alias | Purpose |
| --- | --- | --- |
| `cengine` | `Cheryl::Engine` | Neutral engine contracts and implementation. |
| `module_startup` | `Cheryl::Startup` | Generic application bootstrap, validated command-line configuration and selected backend integration. |
| `module_workers` | `Cheryl::Workers` | Independently selectable hwloc discovery/binding and JSON worker settings; capability-specific legacy aliases refer to this binary. |
| `module_native_glfw` | `Cheryl::NativeGLFW` | Display, windows and optional input. |
| `module_debug_terminal_linux` | `Cheryl::DebugTerminal::Linux` | Optional Linux process output capture/viewer; an interface selects the implementation by configuration. |
| `cengine_debug_terminal_startup` | `Cheryl::DebugTerminal::Startup` | Private static argument/startup attachment accepting a module implementation callback. |
| `module_opengl` | `Cheryl::OpenGL` | Rendering, context, presentation and resources; requires Native GLFW. |
| `module_opengl_startup` (interface) | `Cheryl::OpenGL::Startup` | Compatibility interface linking Startup and OpenGL; produces no additional binary. |
| `module_ui_tgui` | `Cheryl::UI::TGUI` | TGUI widgets and retained scene bridge. |
| `module_ui_rmlui` | `Cheryl::UI::RmlUi` | RML/RCSS documents and retained scene bridge. |
| `module_audio_miniaudio` | `Cheryl::Audio::Miniaudio` | Audio decoding, mixing, native output and streaming. |
| `demo` | — | Example native application. |
| `first-game` | — | Opt-in maintained game from the [walkthrough](docs/site/getting-started/first-game.md), with its own assets. |
| `tutorial-game` | — | Opt-in Controller/Character, joining players, patrol and TGUI HUD example supporting the [tutorials](docs/site/getting-started/create-a/index.md) and [task index](docs/site/getting-started/how-to/index.md). |
| `docs`, `docs-clean`, `docs-tools` | — | Optional incremental generation, full regeneration and shared documentation tool setup; also available through the docs-only CMake project. |
| `cheryl-sdk` | — | Builds selected installable libraries when `CHERYL_BUILD_SDK=ON`. |

See the [module index](projects/modules/README.md) for each owner's API and the
[consumption guide](docs/site/development/consuming-engine.md) for application CMake
composition. Applications can also use an installed prebuilt SDK through
[`find_package(Cheryl 4 CONFIG REQUIRED)`](docs/site/development/consuming-engine.md#prebuilt-development-sdks).

## Tests

With tests enabled, build and run the combined GoogleTest runner for selected owners:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCHERYL_BUILD_TESTS=ON
  cmake --build build/release --target all-tests --parallel
  ./build/release/tests-all
)
```

The [test guide](docs/site/development/testing.md) explains focused runners, aggregates,
CTest selection, consumers and environment opt-ins. Native graphics and real-font
checks need their stated prerequisites; skipped cases leave that coverage pending.
The [testing queue](docs/testing-requests.md) contains pending automated checks and QA.

## CI and development SDKs

[GitHub CI](docs/site/development/ci.md) selects Linux
GCC/Clang and Windows MSVC builds, focused tests and installed SDK consumers.
Successful main-push validation collects prebuilt development SDKs as workflow
artifacts. They contain compiled libraries, public/inline/template headers, CMake
integration and licenses. SDKs use the ordinary build's dependency policy:
embedded libraries stay inside their shared Cheryl owners, shared native runtimes
are packaged or declared as platform requirements, and static Cheryl SDKs also
retain their embedded archive closure for final linking.
The demo and C/C++ implementation files are excluded. GitHub release publication
is separate, and exact hosted Linux/Windows acceptance remains pending the first run.

## Repository architecture

`projects/engine/` owns neutral contracts and implementation. Optional integrations
live under `projects/modules/`; Engine has no dependency on them. Applications
assemble the required owners. `projects/apps/demo/` demonstrates that assembly,
while `projects/tests/` owns cross-module checks.

Each owner declares its own sources, public includes, dependencies and tests. The
root CMake file selects owners; `cmake/` supplies shared metadata and composition
helpers. `extern/` holds pinned dependencies and the `assets/` submodule holds
definitions, textures, shader recipes, UI documents and font inputs. Initialize it
alongside the dependencies; optional package images still follow the
[catalog](docs/site/assets/catalog.md). See [module authoring](docs/site/development/modules.md)
for the steps required to add an owner.

## Documentation and development

From the checkout root, run `./generate-docs.sh` to update the local manual and API
reference. Add `--clean` for full regeneration, `--extract-only` for Doxygen XML,
or `--help` for output and tool-selection options. The
[generation guide](docs/site/development/documentation.md) also covers Python and CMake
entry points and hosted publication.
The generated entry point is `docs/site/generation/generated/html/index.html`.

| Topic                                  | Documentation |
|----------------------------------------| --- |
| Complete documentation index           | [All guides and system references](docs/README.md) |
| Engine systems and integration         | [System relationships](docs/site/engine-systems/index.md), [application integration](docs/site/development/consuming-engine.md), [frame lifecycle](docs/site/runtime/runtime-frame-boundary.md) and [module selection](docs/site/modules/index.md) |
| Engine systems                         | [Purpose, ownership, execution and integration by system](docs/site/engine-systems/index.md) |
| **CMake configuration** and **builds** | [Options and defaults](docs/site/development/building.md#configuration-options), [build configurations](docs/site/development/building.md#build-configurations) and [engine-only builds](docs/site/development/building.md#engine-only-configuration) |
| Dependencies                           | [SDK selection and toolchain settings](docs/site/development/building.md#dependency-and-toolchain-settings), [system requirements](docs/site/development/building.md#system-dependencies) and [public headers and link requirements](docs/site/development/consuming-engine.md#dependencies-and-headers) |
| Targets and artifacts                  | [Module targets and aliases](projects/modules/README.md), [test targets](docs/site/development/testing.md#runner-selection) and [artifact name definitions](cmake/CherylOutputs.cmake) |
| Writing a game                         | [Project setup](docs/site/getting-started/project-setup.md), [Tutorials](docs/site/getting-started/create-a/index.md), [Task index](docs/site/getting-started/how-to/index.md) and [Guides by topic](docs/site/guides/index.md) |
| Documentation generation and hosting   | [Local generation, shared tools and versioned Read the Docs setup](docs/site/development/documentation.md), [generated API entry points](docs/site/api/index.md) |
| Adding a module                        | [Module authoring](docs/site/development/modules.md#add-a-module), [standalone composition](docs/site/development/modules.md#standalone-modules) and [UI adapters](docs/site/development/ui-adapters.md) |
| Platform and graphics APIs             | [Native GLFW](docs/site/modules/native-glfw.md) and [OpenGL](docs/site/modules/opengl.md) |
| UI APIs                                | [TGUI](docs/site/modules/tgui.md) and [RmlUi](docs/site/modules/rmlui.md) |
| Audio APIs                             | [Engine audio contracts](docs/site/runtime/audio.md) and [miniaudio integration](docs/site/modules/miniaudio.md) |
| Input, timing and concurrency          | [Input state and events](docs/site/runtime/input-state-model.md), [simulation timing](docs/site/runtime/simulation-timing.md) and [thread dispatch](docs/site/runtime/thread-dispatch.md) |
| Assets and text                        | [Asset loading](docs/site/assets/asset-loading.md), [manifest format](docs/site/assets/asset-manifests.md) and [Unicode text layout](docs/site/assets/text-layout.md) |
| Rendering and resources                | [Pipelines and materials](docs/site/rendering/pipelines-and-materials.md), [CPU memory lifetime](docs/site/resources/resource-lifetime.md) and [resource residency](docs/site/resources/resource-residency.md) |
| Logging and crash handling             | [Compile-time logging policy](docs/site/runtime/logging.md#compile-policy), [runtime logging](docs/site/runtime/logging.md#runtime-settings-and-ownership) and [crash bootstrap and exception traces](docs/site/development/consuming-engine.md#crash-and-exception-traces) |
| Native output terminal                 | [Linux module, activation and viewer lifetime](docs/site/modules/debug-terminal-linux.md) |
| Demo                                   | [Controls](projects/apps/demo/README.md#controls), [command-line options](projects/apps/demo/README.md#command-line-options) and [asset packages](docs/site/assets/catalog.md) |
| Automated tests                        | [Runners and CTest selection](docs/site/development/testing.md), [acceptance checks](docs/site/development/testing.md#acceptance-and-consumers) and [logging acceptance](docs/site/development/logging-acceptance.md) |
| Consumers and header probes            | [Selection and targets](docs/site/development/testing.md#acceptance-and-consumers) and [independent consumer builds](docs/site/development/architecture-validation.md#independent-consumers-and-header-probes) |
| Development SDKs and CI                | [SDK production](docs/site/development/building.md#development-sdk-builds), [prebuilt consumption](docs/site/development/consuming-engine.md#prebuilt-development-sdks) and [hosted CI matrix](docs/site/development/ci.md) |
| Architecture validation and QA         | [Validation procedures](docs/site/development/architecture-validation.md), [desktop checks](docs/site/development/native-desktop-checks.md) and [pending testing requests](docs/testing-requests.md) |
| Troubleshooting                        | [Build and dependency issues](docs/site/development/building.md#troubleshooting) and [compiler discovery and standard headers](docs/site/development/building.md#standard-headers-in-an-existing-build) |
| Contributing                           | [Development workflow](docs/site/development/contributing.md), [C++ style](docs/site/development/code-style.md) and [agent instructions](AGENTS.md) |
| Planning                               | [Status and selected work](docs/STATUS.md) and [planning catalogue](docs/planning/README.md) |
