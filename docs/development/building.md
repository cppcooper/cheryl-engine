# Configuring and building Cheryl

Use CMake 3.28 or newer, Ninja and a C++23 compiler/standard library with
`std::format`. Initialize the pinned submodules as described in
[setup](../../README.md#setup). Build-tree consumers use the same dependencies;
link Cheryl targets instead of copying include and link settings.

## Configuration options

Pass settings as `-DNAME=value`. These defaults apply to a fresh root configuration;
existing build directories and IDE profiles retain their cache. Native options
are declared only when Native GLFW is selected; the RmlUi verification option is
declared only when RmlUi is selected.

| Option | Default | Effect |
| --- | --- | --- |
| `CHERYL_BUILD_NATIVE_GLFW` | `ON` | Selects GLFW display/windows and optional input. |
| `CHERYL_BUILD_OPENGL` | `ON` | Selects the complete OpenGL backend; requires Native GLFW. |
| `CHERYL_BUILD_UI_TGUI` | `ON` | Selects TGUI independently of platform/graphics. |
| `CHERYL_BUILD_UI_RMLUI` | `OFF` | Selects RmlUi independently; can coexist with TGUI. |
| `CHERYL_RMLUI_PLACEHOLDER_FIX_VERIFIED` | `OFF` | Attests that a supplied RmlUi library includes the placeholder hit-testing correction. Set only after verification; Cheryl-owned source builds declare their correction automatically. |
| `CHERYL_BUILD_AUDIO_MINIAUDIO` | `OFF` | Selects miniaudio decoding, output and streaming. |
| `CHERYL_BUILD_DEMO` | `ON` | Adds `demo` with Native GLFW, OpenGL and native input selected. |
| `CHERYL_BUILD_TESTS` | `ON` | Adds focused tests and explicitly buildable acceptance/aggregate runners. |
| `CHERYL_BUILD_ALL_TESTS` | `OFF` | Includes aggregates in the default build and CTest; requires tests. |
| `CHERYL_BUILD_ACCEPTANCE_TESTS` | `OFF` | Includes acceptance runners in the default build; requires tests. |
| `CHERYL_BUILD_CONSUMER_TESTS` | `OFF` | Adds selected owners' consumers and first-include probes independently of GoogleTest. |
| `CHERYL_NATIVE_INPUT` | `ON` | Includes Gainput-backed input in Native GLFW. |
| `CHERYL_NATIVE_NULL_PLATFORM` | `OFF` | Configures an owned GLFW without X11/Wayland. Disable native input to omit Gainput/X11. |
| `CHERYL_SANDBOX_BUILD` | `OFF` | Deprecated shorthand defaulting native input off and the null platform on. Prefer explicit options. |
| `CHERYL_LOG_PROFILE` | `auto` | Selects `developer`, `support` or `release`; `auto` follows the build configuration. |
| `CHERYL_LOG_COMPILED_MASK` | Empty | Overrides compiled severity bits (`0`–`63`); see [logging policy](../runtime/logging.md#compile-policy). |
| `WARN` | `OFF` | Adds `-Wall`. |

A supplied GLFW target keeps its host's settings. Null-platform OpenGL still needs
OpenGL development/link libraries and does not supply the windowed demo.
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
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
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

## System dependencies

Install development headers and link libraries as well as runtime libraries.
Package names vary by operating system.

| Requirement | Selection | Notes |
| --- | --- | --- |
| Threads | Engine | Discovered through `Threads::Threads`. |
| FreeType, HarfBuzz, ICU uc/i18n | Engine | Font inspection/rasterization, shaping, bidi and boundaries; required with UI disabled too. |
| X11 and extensions | GLFW X11 / native Linux input | GLFW checks XRandR, Xinerama, XKB, Xcursor, XInput and X Shape headers. Gainput input also needs X11. |
| Wayland, xkbcommon, `wayland-scanner` | Bundled GLFW Wayland | Linux GLFW enables X11 and Wayland by default. Disable Wayland for an X11-only build. |
| OpenGL | OpenGL module | Headers/link libraries; native execution also needs a usable driver/display. |
| Python and Jinja2 | GLAD generation | Jinja2 must be available in CMake's selected interpreter. |
| FreeType | TGUI / RmlUi | Each toolkit uses its own font service. |
| libudev / libusb | Linux hidapi | Requirements depend on selected HID backends. |
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
| Standard C++ headers cannot find `math.h` or `stdlib.h` | Inspect stale compiler discovery before editing includes; see [recovery](consuming-engine.md#standard-headers-in-an-existing-build). |

Successful configuration/build does not establish native platform coverage. The
[test guide](testing.md) and [architecture validation](architecture-validation.md)
describe the relevant checks and prerequisites.
