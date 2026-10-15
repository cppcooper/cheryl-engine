# Configuring and building Cheryl

Use CMake 3.28 or newer, Ninja and a C++23 compiler/standard library with
`std::format`. Initialize the pinned submodules as described in
[setup](../../README.md#setup). Build-tree consumers use the same dependencies;
link Cheryl targets instead of copying include and link settings. Prebuilt consumers
use the [SDK import targets](consuming-engine.md#prebuilt-development-sdks).

## Choose the artifact

Configuration selects available targets; an explicit build, install or packaging
command produces the artifact you need. A library build leaves headers in their
source directories. SDK installation collects them into one redistributable
`include/` tree alongside the compiled libraries and CMake import targets.

| Artifact | Trigger | Result |
| --- | --- | --- |
| HTML manual and C++ reference | Build `docs` in the docs-only project below, or enable `CHERYL_BUILD_DOCS` in an engine build. | `docs/generation/generated/html/index.html` and linked pages. |
| Engine library | Build `cengine`; see [engine-only configuration](#engine-only-configuration). | `libcheryl-engine.a` on Linux or `cheryl-engine.lib` on Windows in the build root, plus required dependency artifacts. |
| Selected SDK libraries | Enable `CHERYL_BUILD_SDK`, then build `cheryl-sdk`. | Engine, selected modules/support and their owned dependencies in the build tree. |
| Redistributable SDK directory | Build `cheryl-sdk`, then install the `CherylSDK` component. | One prefix containing `include/`, libraries, CMake package files and licenses. |
| SDK archive with manifest/checksum | Run `.github/ci/package.py` after building `cheryl-sdk`. | Linux `.tar.gz` or Windows `.zip`, `SDK-MANIFEST.json` and `SHA256SUMS`. |

The [SDK commands](#development-sdk-builds) below cover building, installing and
archiving separately. Source consumers do not need to produce an SDK first; linking
`Cheryl::Cheryl` supplies the configured assembly's public include directories and
dependencies for `<cheryl/cheryl.h>`. Leave `CHERYL_BUILD_SDK=OFF` when building an
application from source unless you also want to produce an SDK. This interface
target links the existing libraries; there is no additional combined library
archive. See
[consuming Cheryl](consuming-engine.md) for source and prebuilt integration.

### Build documentation

This uses no C/C++ compiler or engine dependencies. Run from any directory inside
the checkout; the subshell preserves your starting directory:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S docs/generation -B docs/generation/generated/cmake -G Ninja
  cmake --build docs/generation/generated/cmake --target docs
)
```

Open `docs/generation/generated/html/index.html`. Python 3.11 or newer is required;
the generator selects compatible installed tools or prepares pinned tools in the
user cache. Initial tool setup may need network access. See
[documentation generation](documentation.md) for direct Python invocation, tool
selection and output options. The `docs` target prepares its tools automatically.

## Configuration options

Pass settings as `-DNAME=value`. These defaults apply to a fresh root configuration;
existing build directories and IDE profiles retain their cache. Native options
are declared only when Native GLFW is selected; the RmlUi verification option is
declared only when RmlUi is selected.

| Option | Default | Effect |
| --- | --- | --- |
| `CHERYL_BUILD_NATIVE_GLFW` | `ON` | Selects GLFW display/windows and optional input. |
| `CHERYL_BUILD_DEBUG_TERMINAL_LINUX` | Linux GNU/Clang: `ON`; elsewhere: `OFF` | Selects the optional Linux output-terminal module for root applications/test runners. |
| `CHERYL_DEBUG_TERMINAL` | `AUTO` | Links the selected terminal only in Debug; `ON` includes other configurations, where display requires `--debug-terminal`; `OFF` omits it. |
| `CHERYL_DEBUG_TERMINAL_EMULATOR` | `auto` | Linux module launcher selection: `auto`, `konsole` or `xterm`. |
| `CHERYL_BUILD_OPENGL` | `ON` | Selects the complete OpenGL backend; requires Native GLFW. |
| `CHERYL_BUILD_UI_TGUI` | `ON` | Selects TGUI independently of platform/graphics. |
| `CHERYL_BUILD_UI_RMLUI` | `OFF` | Selects RmlUi independently; can coexist with TGUI. |
| `CHERYL_RMLUI_PLACEHOLDER_FIX_VERIFIED` | `OFF` | Attests that a supplied RmlUi library includes the placeholder hit-testing correction. Set only after verification; Cheryl-owned source builds declare their correction automatically. |
| `CHERYL_BUILD_AUDIO_MINIAUDIO` | `OFF` | Selects miniaudio decoding, output and streaming. |
| `CHERYL_BUILD_DEMO` | `ON` | Adds `demo` with Native GLFW, OpenGL and native input selected. |
| `CHERYL_BUILD_FIRST_GAME` | `OFF` | Adds the maintained `first-game` walkthrough application with Native GLFW, OpenGL and native input selected. |
| `CHERYL_BUILD_DOCS` | `OFF` | Adds the optional `docs-tools` and `docs` targets; tools are selected when these targets run. |
| `CHERYL_BUILD_TESTS` | `ON` | Adds focused tests and explicitly buildable acceptance/aggregate runners. |
| `CHERYL_BUILD_ALL_TESTS` | `OFF` | Includes aggregates in the default build and CTest; requires tests. |
| `CHERYL_BUILD_ACCEPTANCE_TESTS` | `OFF` | Includes acceptance runners in the default build; requires tests. |
| `CHERYL_BUILD_CONSUMER_TESTS` | `OFF` | Adds selected owners' consumers and first-include probes independently of GoogleTest. |
| `CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC` | `OFF` | Selects optional runtime topology discovery and thread binding through hwloc. |
| `CHERYL_BUILD_WORKER_TOPOLOGY_JSON` | `OFF` | Selects the versioned worker settings loader independently of hwloc. |
| `CHERYL_BUILD_SDK` | `OFF` | Adds the `cheryl-sdk` build target and `CherylSDK` install/export component for selected libraries, headers and dependencies. |
| `CHERYL_NATIVE_INPUT` | `ON` | Includes Gainput-backed input in Native GLFW. |
| `CHERYL_NATIVE_NULL_PLATFORM` | `OFF` | Configures an owned GLFW without X11/Wayland. Disable native input to omit Gainput/X11. |
| `CHERYL_SANDBOX_BUILD` | `OFF` | Deprecated shorthand defaulting native input off and the null platform on. Prefer explicit options. |
| `CHERYL_LOG_PROFILE` | `auto` | Selects `developer`, `support` or `release`; `auto` follows the build configuration. |
| `CHERYL_LOG_COMPILED_MASK` | Empty | Overrides compiled severity bits (`0`–`63`); see [logging policy](../runtime/logging.md#compile-policy). |
| `WARN` | `OFF` | Adds `-Wall`. |

A supplied GLFW target keeps its host's settings. Null-platform OpenGL still needs
OpenGL development/link libraries and does not supply the windowed demo.
Cheryl disables documentation generation for its owned GLFW dependency, so ordinary
engine configuration does not probe Doxygen for GLFW's manual.
Standalone modules default their local `CHERYL_BUILD_TESTS` to `OFF`; the
[module guide](modules.md#standalone-modules) covers their composition.

### Build configurations

Keep different configurations in separate directories. This block builds Debug;
substitute `RelWithDebInfo`/`build/support` or `Release`/`build/release` as needed:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
  cmake --build build/debug --target demo --parallel
)
```

Non-MSVC Debug enables AddressSanitizer and UndefinedBehaviorSanitizer. Automatic
logging selects developer in Debug, support in RelWithDebInfo and release otherwise.
All translation units sharing Cheryl headers must use the same compiled logging
definitions; runtime levels cannot restore stripped severities.

Executables and archives use the configured build root. Windows adds `.exe` to
executables. Multi-configuration generators require `--config Release` for builds
and `-C Release` for CTest, with executables normally in `Release/`.
Target and artifact declarations are centralized in
[CherylTargets.cmake](../../cmake/CherylTargets.cmake) and
[CherylOutputs.cmake](../../cmake/CherylOutputs.cmake).

### Engine-only configuration

This omits GLFW, Gainput, OpenGL, GLAD, UI toolkits and miniaudio. Engine still needs
its text dependencies:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/engine-only -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
    -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
    -DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=OFF -DCHERYL_BUILD_WORKER_TOPOLOGY_JSON=OFF \
    -DCHERYL_BUILD_DEMO=OFF -DCHERYL_BUILD_TESTS=OFF
  cmake --build build/engine-only --target cengine --parallel
)
```

## Dependency and toolchain settings

| Setting | Purpose |
| --- | --- |
| `GLFW_BUILD_WAYLAND=OFF` | Uses bundled GLFW's X11 backend on Linux without Wayland development tools. |
| `GAINPUT_ENABLE_HID=OFF` | Omits Gainput's HID support and hidapi fetch. Use the [current Linux joystick path](../../projects/modules/platform/native-glfw/README.md#controller-backend-limits) while HID integration remains unresolved. |
| `Python_EXECUTABLE=/path/to/python` | Selects GLAD's interpreter; Jinja2 must be installed in it. |
| `CMAKE_POLICY_VERSION_MINIMUM=3.5` | Allows pinned legacy dependency files to configure with CMake 4 when needed. |
| `CHERYL_REPOSITORY_ROOT=/path/to/cheryl-engine` | Supplies shared helpers and Engine bootstrapping for standalone modules/consumers. Root configuration sets it automatically. |
| `CHERYL_NATIVE_GLFW_SOURCE=/path/to/native-glfw` | Supplies Native GLFW to standalone OpenGL when its target is absent. |
| `CHERYL_TGUI_SOURCE`, `TGUI_DIR` | Selects a TGUI 1.13.0 source tree or exact package with custom/FreeType support. |
| `CHERYL_RMLUI_SOURCE`, `RmlUi_DIR` | Selects a RmlUi 6.3 source tree or exact package with stock FreeType and the required [placeholder correction](../../projects/modules/ui/rmlui/README.md#placeholder-dependency-contract). |
| `CHERYL_MINIAUDIO_SOURCE` | Overrides pinned miniaudio source; a supplied miniaudio target takes precedence. |
| `CHERYL_RMLUI_TEST_FONT=/path/to/font.ttf` | Supplies a real font fixture when the selected SDK's sample font is unavailable. |
| `CHERYL_HWLOC_TARGET=existing_target` | Supplies a compatible hwloc 2.5+ (2.x) target when worker discovery/binding is selected; otherwise discovery checks supported config targets and pkg-config. |

Modules reuse compatible supplied dependency targets where supported. Their owner
guides document selection order and feature requirements. Owned source dependencies
are pinned in [.gitmodules](../../.gitmodules); initialize the recorded revisions.
The first HID-enabled configuration fetches hidapi 0.15.0 and needs network access
or a prepared FetchContent cache. UI dependency selection performs no download.

Engine configuration registers `Cheryl::Startup`, reusing `CLI11::CLI11` or adding
the pinned `extern/cli11` source. OpenGL registers `Cheryl::OpenGL::Startup`.
These support libraries are excluded from the default build unless a consumer
links them or names their build targets; the demo links OpenGL startup. Plain
Engine/OpenGL targets do not acquire CLI11 usage requirements. See
[application startup](consuming-engine.md#command-line-startup).

The terminal module adds a separate optional startup target and linker entry hook;
plain Engine links neither. Debug display requires a desktop and supported emulator;
missing prerequisites fall back to inherited output. Headless Debug and test
discovery create no viewer. See the
[Linux terminal guide](../../projects/modules/platform/debug-terminal-linux/README.md)
for output/report separation and lifetime policy. `ON` is a build inclusion option,
not a request to display a Release window automatically.

## System dependencies

Install development headers and link libraries as well as runtime libraries.
Package names vary by operating system.

| Requirement | Selection | Notes |
| --- | --- | --- |
| Threads | Engine | Discovered through `Threads::Threads`. |
| hwloc 2.5+ (2.x) | WorkerTopology::Hwloc only | Runtime CPU/core/cache discovery and calling-thread binding; omitted configurations do not discover/download/link it. |
| FreeType, HarfBuzz, ICU uc/i18n | Engine | Font inspection/rasterization, shaping, bidi and boundaries; required with UI disabled too. |
| X11 and extensions | GLFW X11 / native Linux input | GLFW checks XRandR, Xinerama, XKB, Xcursor, XInput and X Shape headers. Gainput input also needs X11. |
| Wayland, xkbcommon, `wayland-scanner` | Bundled GLFW Wayland | Linux GLFW enables X11 and Wayland by default. Disable Wayland for an X11-only build. |
| OpenGL | OpenGL module | Headers/link libraries; native execution also needs a usable driver/display. |
| Python and Jinja2 | GLAD generation | Jinja2 must be available in CMake's selected interpreter. |
| FreeType | TGUI / RmlUi | Each toolkit uses its own font service. |
| libudev / libusb / Iconv | Linux hidapi | Requirements depend on selected HID backends; SDK consumers need the selected backend's native libraries too. |
| Konsole or xterm | Linux output terminal at runtime | A desktop is needed for display; neither is an Engine build dependency. |
| libdw, libbfd, or libdwarf/libelf | Backward, optional | Improve trace source/symbol resolution. |

Engine source dependencies include GLM, CTTI, spdlog, Backward, STB and JSON.
Integration SDKs belong to their modules; GoogleTest is test-only. See
[consumer usage requirements](consuming-engine.md#dependencies-and-headers) and the
[module index](../../projects/modules/README.md) for dependency ownership.

## Troubleshooting

| Symptom | Next step |
| --- | --- |
| Selected dependency directory is empty | Initialize the pinned submodules. |
| A selected target is missing in CLion | Change the cached option and reload CMake. |
| Python cannot import `jinja2` | Install it in the selected interpreter or set `Python_EXECUTABLE`. |
| GLFW cannot find Wayland tools/libraries | Install the requirements or set `GLFW_BUILD_WAYLAND=OFF`. |
| CMake 4 rejects an old dependency policy | Set `CMAKE_POLICY_VERSION_MINIMUM=3.5`. |
| Supplied RmlUi Core lacks the placeholder-correction declaration | Use Cheryl-owned source or provide verified target/configuration metadata according to the [dependency contract](../../projects/modules/ui/rmlui/README.md#placeholder-dependency-contract). |
| Standard C++ headers cannot find `CE::Math.h` or `stdlib.h` | Inspect stale compiler discovery before editing includes; see [recovery](consuming-engine.md#standard-headers-in-an-existing-build). |

Successful configuration/build does not establish native platform coverage. The
[test guide](testing.md) and [architecture validation](architecture-validation.md)
describe the relevant checks and prerequisites.

Documentation has a separate [generation entry point](documentation.md) that
does not configure or compile the engine. The docs-only CMake project exposes
the same targets without graphics or native development dependencies.

## Optional worker support

`CHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=ON` adds `cengine_worker_topology_hwloc`
(`Cheryl::WorkerTopology::Hwloc`); JSON selection adds `cengine_worker_topology_json`
(`Cheryl::WorkerTopology::Json`). Both options default off. A plain Engine consumer
needs neither support library. Selecting both also registers the explicitly
buildable `example_worker_topology` shared-pool composition example.
The [worker guide](../runtime/worker-execution.md) owns provider lifetime, placement
policies, capacity and configuration semantics.

hwloc selection reuses an explicit or compatible existing target, then an installed
config package or pkg-config SDK. Missing/incompatible selection fails clearly;
there is no implicit SDK download/install. Cross builds require a supplied target
SDK or target-rooted pkg-config settings (`PKG_CONFIG_LIBDIR` and
`PKG_CONFIG_SYSROOT_DIR`); do not resolve host libraries accidentally. Discovery
occurs when the application creates its provider, never through `lstopo` at configure
time. Builds do not bake host CPU/cache layout into artifacts or add `-march=native`.
CPU vendor is not a CMake selection.

## Development SDK builds

SDK export is opt-in and requires Cheryl-owned builds of the pinned source
dependencies; imported dependency targets are rejected by this packaging path,
except an imported hidapi package that remains external to the SDK.
Ordinary source composition can still reuse supplied targets. Use a clean checkout
with submodules at their recorded revisions for reproducible archives. The packager
records the source commit and verifies recursive submodule HEADs against its gitlinks.

Use a single-configuration Ninja build and select the modules/helpers to distribute.
On Linux, this builds the default Native GLFW/OpenGL/TGUI libraries and common
Startup support without building the demo or tests:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/sdk -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_INSTALL_LIBDIR=lib -DCHERYL_BUILD_SDK=ON \
    -DCHERYL_BUILD_DEMO=OFF -DCHERYL_BUILD_TESTS=OFF \
    -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF \
    -DGAINPUT_ENABLE_HID=OFF -DGLFW_BUILD_WAYLAND=OFF
  cmake --build build/sdk --target cheryl-sdk --parallel
)
```

`cheryl-sdk` builds the selected libraries and their dependencies. It does not
copy headers, install a directory or create an archive. With native input and HID
enabled, SDK export includes the owned hidapi backend selected by Gainput, its
headers and licenses. Linux consumers still need its selected libudev/libusb and
Iconv dependencies; the package config discovers them. This packaging support does
not change the [Linux controller limitations](../../projects/modules/platform/native-glfw/README.md#controller-backend-limits).
To collect the libraries and redistributable headers into a local SDK prefix after
that build:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake --install build/sdk --component CherylSDK --prefix "$PWD/build/sdk-prefix"
)
```

The prefix contains `include/`, `lib/`, `lib/cmake/Cheryl/` and `share/licenses/`
with the configuration above. Windows may also install runtime libraries under
`bin/`. The install command does not compile missing libraries; run the build step
first. Consumers use `find_package(Cheryl CONFIG REQUIRED)` with this prefix in
`CMAKE_PREFIX_PATH`; see [SDK consumption](consuming-engine.md#prebuilt-development-sdks).

For a complete distributable archive, use Python 3.12 or newer:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  python3 .github/ci/package.py --build-dir build/sdk --output build/packages
)
```

`package.py` performs its own temporary install of only the `CherylSDK` component,
creates a Linux `.tar.gz` or Windows `.zip` under `build/packages/`, and writes
`SHA256SUMS` plus `SDK-MANIFEST.json` there. You can skip the direct install command
when you only need an archive. Windows archive production additionally copies its
native dependency prefix; use `package.py` for the complete Windows bundle.
Archive names include the Cheryl version from
[CherylVersions.cmake](../../cmake/CherylVersions.cmake), platform/distribution,
architecture, compiler/version, configuration and commit ID.

Windows CI builds through the pinned vcpkg manifest in [.github/ci](../../.github/ci/vcpkg.json)
with the `x64-windows` triplet. Packaging copies release headers, import libraries,
DLLs, CMake files and licenses from `build/ci/vcpkg_installed/x64-windows` into
`dependencies/`; `--native-prefix` can select another prepared prefix. Debug
dependency trees, tools and applications are omitted. Linux retains system native
dependencies. Neither package contains the demo or C/C++ implementation translation
units; public and inline/template headers remain included. The selected libraries
are already compiled. See [SDK consumption](consuming-engine.md#prebuilt-development-sdks)
for ABI, logging and runtime-library requirements.

## GitHub CI and SDK artifacts

[ci.yml](../../.github/workflows/ci.yml) validates pushes to `develop`/`main`, pull
requests targeting either branch and manual dispatches. Each job checks out recorded
recursive submodules. Actions use commit pins; Windows uses the manifest's pinned
vcpkg baseline. Builds use Ninja and available CPU parallelism.

| Job | Host/compiler/configuration | Selection |
| --- | --- | --- |
| `linux-gcc-hwloc` | Ubuntu 24.04 x64, GCC 14 Release | Headless Engine, hwloc and JSON. |
| `linux-gcc-neutral` | Ubuntu 24.04 x64, GCC 14 Release | Headless Engine and JSON, hwloc omitted. |
| `linux-clang-debug` | Ubuntu 24.04 x64, Clang 18 Debug | Headless Engine, hwloc/JSON, ASan/UBSan. |
| `windows-msvc-hwloc` | Windows 2025 x64, MSVC RelWithDebInfo | Headless Engine and hwloc/JSON with vcpkg native libraries. |
| `linux-modules` | Ubuntu 24.04 x64, GCC 14 Release | Native GLFW/input, OpenGL, TGUI, RmlUi, miniaudio, hwloc/JSON and SDK. |
| `windows-modules` | Windows 2025 x64, MSVC RelWithDebInfo | The same selected modules/helpers and SDK, with bundled native dependencies. |

[run.py](../../.github/ci/run.py) configures explicit owners/options/build targets,
runs each focused owner prefix once and executes independent consumers. Modules
jobs compile the demo for composition coverage but never launch or package it.
Linux jobs provide TrueType/CFF allocation fixtures. Missing fixtures and
platform-specific skips are reported as unaccepted coverage. Native OpenGL, native
hwloc binding acceptance, actual audio devices, desktop/controller QA and the
Debug terminal remain separate from this focused CI selection.

Modules jobs package and verify an extracted SDK on every validation run.
[verify_sdk.py](../../.github/ci/verify_sdk.py) checks the checksum and absence of
producer paths, then builds independent consumers/header probes using only installed
Cheryl libraries. Test fixture sources/fonts come from the checkout; Cheryl and
module implementation sources are not rebuilt. Use a fresh consumer build/extraction
directory for each verification. See
[repeating SDK checks](architecture-validation.md#installed-sdk-consumers).

Successful main-push modules jobs retain SDK candidates for three days. After all
headless and modules jobs succeed, the collection job verifies checksums and uploads
`cheryl-development-sdks-<commit>` for 90 days. Test/configuration reports are retained
for 14 days. These are workflow artifacts; the workflow creates no GitHub release,
uploads no release assets and publishes no demo package. Release publication is a
separate future decision.

The selected hosted Ubuntu GCC/Clang and Windows MSVC producer jobs and relocated
SDK consumers are accepted. The first main-push artifact collection remains
[TR20](../testing-requests.md#tr20-automated-main-push-sdk-collection); collection is
intentionally skipped on PR, `develop` and manual runs.

### Hosted Windows coverage limits

Both Windows profiles leave these focused cases unaccepted:

| Cases | Missing prerequisite and next action |
| --- | --- |
| `font_bake.real_nested_allocation_scope`, `font_bake.real_cff_alloc_failure` | The workflow supplies the bundled TrueType face but no CFF OpenType fixture on Windows. Supply `CHERYL_STB_ALLOCATION_CFF` with a suitable face, retain the TrueType fixture for nested scopes, and rerun these cases on hosted Windows. |
| `logging.config_reopen` | The test needs a temporary directory on the checkout's filesystem root so it can use a relative log path. Configure the hosted Windows temporary directory on that drive, then rerun the case. |

The [font fixture requirements](architecture-validation.md#native-and-fixture-checks)
define suitable faces. A green job does not accept these skipped cases; successful
local MSVC/Wine checks do not replace the missing hosted execution. These limits
are separate from SDK collection and desktop/GPU/device acceptance.
