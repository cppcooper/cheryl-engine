# Cheryl Engine

Cheryl is a C++23 engine with one neutral engine library and optional platform,
graphics, UI and audio modules. It provides sequential or concurrent simulation,
retained 2D rendering, input actions/events/text, Unicode layout and CPU asset
preparation separate from graphics upload. Game state and mechanics belong to the
application.

The default assembly includes Native GLFW, OpenGL, TGUI and the windowed demo.
RmlUi and miniaudio are independently selectable and default to `OFF`.

## Contents

- [Documentation and development](#documentation-and-development)
- [Setup](#setup)
- [Targets](#targets)
- [Tests](#tests)
- [Repository architecture](#repository-architecture)

## Setup

You need Git, CMake 3.28 or newer, Ninja, and a C++23 compiler and standard library
with `std::format` support. Install the selected modules'
[system dependencies](docs/development/building.md#system-dependencies).

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
  cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build/release --target demo --parallel
  ./build/release/demo
)
```

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

The [build guide](docs/development/building.md) lists configuration options,
dependency settings, Debug/Release behavior and common setup failures. Existing
build directories retain cached options; reload CMake after changing selections.

## Targets

Build concrete target names with `cmake --build ... --target`; applications link
the public aliases to inherit headers and dependencies.

| Build target | Public alias | Purpose |
| --- | --- | --- |
| `cengine` | `Cheryl::Engine` | Neutral engine contracts and implementation. |
| `module_native_glfw` | `Cheryl::NativeGLFW` | Display, windows and optional input. |
| `module_opengl` | `Cheryl::OpenGL` | Rendering, context, presentation and resources; requires Native GLFW. |
| `module_ui_tgui` | `Cheryl::UI::TGUI` | TGUI widgets and retained scene bridge. |
| `module_ui_rmlui` | `Cheryl::UI::RmlUi` | RML/RCSS documents and retained scene bridge. |
| `module_audio_miniaudio` | `Cheryl::Audio::Miniaudio` | Audio decoding, mixing, native output and streaming. |
| `demo` | — | Example native application. |

See the [module index](projects/modules/README.md) for each owner's API and the
[consumption guide](docs/development/consuming-engine.md) for application CMake
composition. Build-tree composition is supported; installed
`find_package(Cheryl)` packaging remains future work.

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

The [test guide](docs/development/testing.md) explains focused runners, aggregates,
CTest selection, consumers and environment opt-ins. Native graphics and real-font
checks need their stated prerequisites; skipped cases leave that coverage pending.
The [testing queue](docs/testing-requests.md) contains outstanding user-run QA.

## Repository architecture

`projects/engine/` owns neutral contracts and implementation. Optional integrations
live under `projects/modules/`; Engine has no dependency on them. Applications
assemble the required owners. `projects/apps/demo/` demonstrates that assembly,
while `projects/tests/` owns cross-module checks.

Each owner declares its own sources, public includes, dependencies and tests. The
root CMake file selects owners; `cmake/` supplies shared metadata and composition
helpers. `extern/` holds pinned dependencies and `assets/` holds manifests,
shaders and small fixtures. See [module authoring](docs/development/modules.md)
for the steps required to add an owner.

## Documentation and development

| Topic | Documentation |
| --- | --- |
| Complete documentation index | [All guides and system references](docs/README.md) |
| Engine architecture | [Runtime ownership and backend boundaries](docs/runtime/runtime-architecture.md), [frame lifecycle](docs/runtime/runtime-frame-boundary.md) and [module roles](projects/modules/README.md) |
| CMake configuration and builds | [Options and defaults](docs/development/building.md#configuration-options), [build configurations](docs/development/building.md#build-configurations) and [engine-only builds](docs/development/building.md#engine-only-configuration) |
| Dependencies | [SDK selection and toolchain settings](docs/development/building.md#dependency-and-toolchain-settings), [system requirements](docs/development/building.md#system-dependencies) and [public headers and link requirements](docs/development/consuming-engine.md#dependencies-and-headers) |
| Targets and artifacts | [Module targets and aliases](projects/modules/README.md), [test targets](docs/development/testing.md#runner-selection) and [artifact name definitions](cmake/CherylOutputs.cmake) |
| Writing a game | [Application composition](docs/development/consuming-engine.md), [game hooks and ownership](docs/development/consuming-engine.md#game-hooks-and-ownership) and [minimal native application](docs/development/consuming-engine.md#minimal-native-application) |
| Adding a module | [Module authoring](docs/development/modules.md#add-a-module), [standalone composition](docs/development/modules.md#standalone-modules) and [UI adapters](docs/development/ui-adapters.md) |
| Platform and graphics APIs | [Native GLFW](projects/modules/platform/native-glfw/README.md) and [OpenGL](projects/modules/graphics/opengl/README.md) |
| UI APIs | [TGUI](projects/modules/ui/tgui/README.md) and [RmlUi](projects/modules/ui/rmlui/README.md) |
| Audio APIs | [Engine audio contracts](docs/runtime/audio.md) and [miniaudio integration](projects/modules/audio/miniaudio/README.md) |
| Input, timing and concurrency | [Input state and events](docs/runtime/input-state-model.md), [simulation timing](docs/runtime/simulation-timing.md) and [thread dispatch](docs/runtime/thread-dispatch.md) |
| Assets and text | [Asset loading](docs/assets/asset-loading.md), [manifest format](docs/assets/asset-manifests.md) and [Unicode text layout](docs/assets/text-layout.md) |
| Rendering and resources | [Pipelines and materials](docs/rendering/pipelines-and-materials.md), [resource lifetime](docs/resources/resource-lifetime.md) and [consumer resource contracts](docs/resources/consumer-resource-contract.md) |
| Logging and crash handling | [Compile-time logging policy](docs/runtime/logging.md#compile-policy), [runtime logging](docs/runtime/logging.md#runtime-settings-and-ownership) and [crash bootstrap and exception traces](docs/development/consuming-engine.md#crash-and-exception-traces) |
| Demo | [Controls](projects/apps/demo/README.md#controls), [command-line options](projects/apps/demo/README.md#command-line-options) and [asset packages](docs/assets/catalog.md) |
| Automated tests | [Runners and CTest selection](docs/development/testing.md), [acceptance checks](docs/development/testing.md#acceptance-and-consumers) and [logging acceptance](docs/development/logging-acceptance.md) |
| Consumers and header probes | [Selection and targets](docs/development/testing.md#acceptance-and-consumers) and [independent consumer builds](docs/development/consuming-engine.md#independent-consumers-and-header-probes) |
| Architecture validation and QA | [Validation procedures](docs/development/architecture-validation.md), [desktop checks](docs/development/native-desktop-checks.md) and [pending testing requests](docs/testing-requests.md) |
| Troubleshooting | [Build and dependency issues](docs/development/building.md#troubleshooting) and [compiler discovery and standard headers](docs/development/consuming-engine.md#standard-headers-in-an-existing-build) |
| Contributing | [Development workflow](docs/development/contributing.md), [C++ style](docs/development/code-style.md) and [agent instructions](AGENTS.md) |
| Planning | [Planning catalogue](docs/planning/README.md) and [development roadmap](docs/planning/develop-review-and-development-plan.md) |
