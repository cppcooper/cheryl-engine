# Architecture validation

This guide describes reusable checks for the selected Engine, Native GLFW and
OpenGL assembly. [The module guide](modules.md) defines target ownership, public
usage requirements, selection options and test runners. Use these procedures when
changes affect isolation, composition or consumer contracts; coverage limits remain
separate from source completion.

## Coverage and selection

| Selection | Required checks |
| --- | --- |
| Engine only | No Cheryl OpenGL/GLAD/GLFW/Gainput/native X11 discovery, compilation or linkage. The Engine consumer and neutral first-include probes inherit public requirements without handwritten paths. Unit tests use real Engine code with small contract adapters, without native modules, displays or devices. Measure their build/run cost separately from broader acceptance. |
| Engine + Native GLFW | Native consumer/probes and implementation cases link the selected owner. No Cheryl OpenGL backend, GLAD generation or explicit OpenGL package requirement is selected; upstream GLFW retains its context machinery. Native-only selection does not promise a window-only runtime. |
| Engine + Native GLFW + OpenGL | OpenGL consumer/probes, module implementation cases and selected owner/combined runner registration are correct. Native startup, presentation, retained-resource cleanup, input backpressure and concurrent shutdown require their explicit opt-ins. |
| Standalone and supplied targets | Each module reuses one Engine; OpenGL reuses one Native GLFW. Dependency bootstrapping does not recursively enable root modules, tests or demo. Consumers link their actual owners and CTest discovers module-local cases. |
| Crash bootstrap | An Engine-only consumer referencing no Engine entry point receives the automatic bootstrap object. Check the consumer's actual `NDEBUG` scope independently of exception/explicit traces or standalone Backward tests. |

Controlled Engine tests establish runtime, publication and failure behavior against
predictable dependencies. Implementing modules own checks of their actual behavior
and contract compliance; passing with a dummy does not establish module conformance.
Broader Engine cases cover dispatch/cancellation, events, workers, timing, caches,
retained frames and failure paths. Owner runners and the combined runner reuse case
objects, so choose one runner or CTest prefix for each scenario rather than executing
the same cases through every aggregate.

## Repeating validation

Use the toolchain and selected-owner prerequisites in the [build guide](building.md).
An existing combined build cannot prove Engine-only isolation. Use a fresh directory
when an independent graph is needed, and inspect target dependencies, compilation
inventories and final links. Reuse matching compiled targets for unaffected checks.

Batch required build targets and select each case once. Use isolated working
directories for checks that share mutable files. `CMAKE_CXX_SCAN_FOR_MODULES=OFF`
avoids C++ language-module scanning; Cheryl integration modules are libraries.
`CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST` keeps case discovery out of
build execution. [Runner selection](testing.md) explains focused and aggregate
registration.

### Engine only

A fresh Release configuration provides a neutral graph without sandbox mode:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build-validation-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_LOG_PROFILE=developer \
    -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF \
    -DCHERYL_BUILD_DEMO=OFF -DCHERYL_SANDBOX_BUILD=OFF
  cmake --build build-validation-release --parallel --target \
    consumer-cengine tests-engine tests-logging \
    acceptance-logging acceptance-signal
  ./build-validation-release/cheryl-consumer
  ./build-validation-release/tests-engine
)
```

Consumers depend on their first-include probes; no separate probe build is needed.
Use the logging and Release signal drivers below. The logging driver also executes
focused logging cases, so they need no additional unit run.

### Engine asset and text regressions

The Linux Release Engine-only selection covers neutral clip, tile selection/animation,
manifest, UTF-8, legacy scalar fallback, owned font selection/layout and text-resource
regressions plus public first-include probes. It uses immutable synthetic resources,
the bundled font and isolated discovery roots; no display, installed font, controller
or audio device is needed. These CPU cases do not establish native text rendering,
artwork appearance, device output or other platforms.

Reuse `build/testing-engine` when its toolchain/configuration matches. When a relevant
change requires a rerun, batch the runner and public probes and select only the
affected cases; avoid repeating them through aggregates. This complete focused
selection is reusable from any directory inside the checkout and preserves the
caller's working directory:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S . -B build/testing-engine -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
    -DCHERYL_LOG_PROFILE=developer -DCHERYL_SANDBOX_BUILD=OFF \
    -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
    -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
    -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
    -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
    -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=ON \
    -DCHERYL_BUILD_DEMO=OFF
  cmake --build build/testing-engine --parallel "$(nproc)" --target \
    tests-engine consumer-headers-cengine
  ctest --test-dir build/testing-engine --parallel "$(nproc)" --output-on-failure \
    --no-tests=error \
    -R '^tests-engine\.((audio_clip|tile_selection|tileset_selection|tile_animation|asset_manifest|utf8|stbfont|font_selection|text_layout|text_resources)\.|asset_submission\.(selected_tile|tile_strip_ranges|text_layout_and_retention|utf8_text)$)'
)
```

### Typed events

For Engine-only typed-event checks, enable `CHERYL_BUILD_ACCEPTANCE_TESTS` alongside
the unit and consumer selections above and add `acceptance-engine` to the needed
build targets. The consumer and first-include probes cover the neutral typed API.
Run the focused unit and acceptance selections:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./build-validation-release/cheryl-consumer
  ./build-validation-release/tests-engine --gtest_filter=typed_events.*
  ./build-validation-release/tests-acceptance-engine \
    --gtest_filter='typed_events.*:event_delivery.runtime_owner_threads'
)
```

The [unit cases](../../projects/engine/tests/unit/src/typed-events.cpp) cover exact
identity, type constraints, owned payloads, removal, nested dispatch and immediate
errors. The [acceptance cases](../../projects/engine/tests/acceptance/src/typed-events.cpp)
cover deferred copies, cancellation, error-sink ownership, waits and worker FIFO.
`event_delivery.runtime_owner_threads` exercises typed resize payloads on platform
and simulation owners in sequential and concurrent runtime modes without borrowing
window access across owners.

Select Native GLFW + OpenGL with native input and OpenGL acceptance enabled for the
native resize bridge. With a usable display, run its focused owner cases:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  CHERYL_NATIVE_GL_TESTS=1 ./build-opengl-module/tests-acceptance-opengl \
    --gtest_filter='native_opengl.resize_events:native_opengl.typed_resize_failure:native_opengl.resize_callback_failure'
)
```

These cases invoke the actual registered C callback to check unchanged-size
suppression, saved observations across nested resize delivery, legacy-before-typed
offers and native failure consumption. They do not establish compositor-generated
resize delivery. Skipped native cases leave that callback coverage pending. Windows execution
is shelved in the [platform plan](../planning/platform-acceptance.md).

### Native, OpenGL and standalone composition

The [standalone entry points](modules.md#standalone-modules) can prove both the
selected dependency graphs and module bootstrap without building each assembly
twice. Use the same Release, logging and discovery settings as above. Enable
`CHERYL_BUILD_TESTS`, `CHERYL_BUILD_CONSUMER_TESTS` and `CHERYL_BUILD_ALL_TESTS`
for the selected module. Build its targets together:

| Entry point | Targets |
| --- | --- |
| `projects/modules/platform/native-glfw/` | `consumer-module-native-glfw`, `tests-native-glfw`, `all-native-glfw` |
| `projects/modules/graphics/opengl/` | `consumer-module-opengl`, `tests-opengl`, `acceptance-opengl`, `all-opengl` |

Supply `CHERYL_REPOSITORY_ROOT` explicitly; OpenGL also accepts
`CHERYL_NATIVE_GLFW_SOURCE`. For Linux/X11 native input, select
`CHERYL_NATIVE_INPUT=ON`, `CHERYL_NATIVE_NULL_PLATFORM=OFF`, `GLFW_BUILD_X11=ON`
and `GLFW_BUILD_WAYLAND=OFF`. Bootstrapped Engine tests remain disabled; OpenGL's
Native bootstrap must not select Native tests/consumers. Run each selected consumer
and its needed cases, and inspect module-local CTest registration with `ctest -N`.

For supplied-target reuse, an independent host can compose the owners directly:

```cmake
cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD 23)
project(cheryl_module_reuse LANGUAGES C CXX)
enable_testing()

set(CHERYL_BUILD_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
block(SCOPE_FOR VARIABLES)
    set(CHERYL_BUILD_NATIVE_GLFW OFF)
    set(CHERYL_BUILD_OPENGL OFF)
    set(CHERYL_BUILD_UI_TGUI OFF)
    set(CHERYL_BUILD_UI_RMLUI OFF)
    set(CHERYL_BUILD_TESTS OFF)
    set(CHERYL_BUILD_CONSUMER_TESTS OFF)
    set(CHERYL_BUILD_DEMO OFF)
    add_subdirectory("${CHERYL_REPOSITORY_ROOT}" engine EXCLUDE_FROM_ALL)
endblock()

set(CHERYL_BUILD_TESTS ON)
set(CHERYL_BUILD_CONSUMER_TESTS ON)
set(CHERYL_BUILD_ALL_TESTS ON)
add_subdirectory("${CHERYL_NATIVE_GLFW_SOURCE}" native-glfw)
add_subdirectory("${CHERYL_OPENGL_SOURCE}" opengl)
```

Use explicit source paths and the same compiler/Release/logging settings. The host
owns root `enable_testing()` policy. Verify actual consumer links, local CTest
registration, one target per owner and no duplicate dependency bootstrap directories.
Source-target composition does not prove installed/imported package support.

### Neutral UI consumer

`ui_probe.retained_scene` uses Engine alone with controlled display, input, resources
and rendering in both runtime modes. It checks ordered colored/clipped packets,
keyboard/text focus with controller gameplay, queued immutable image replacement
and retained resources through teardown. It supplies synthetic ASCII metrics to
legacy STBFont, so it needs no host font and does not establish Unicode appearance.

With the Engine-only configuration above and acceptance selected, run:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake --build build-validation-release --parallel --target acceptance-engine
  ./build-validation-release/tests-acceptance-engine --gtest_filter='ui_probe.*:runtime_adapter.*'
)
```

The separate `native_opengl.ui_clipping_color` case checks real straight-alpha
pixels and scaled clipping. Use the native opt-in below. Neither controlled proof
establishes toolkit behavior, physical DPI/compositor behavior or IME; selected
adapters own their checks.

## Native and fixture checks

Native OpenGL cases require `CHERYL_NATIVE_GL_TESTS=1` and a usable display:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  CHERYL_NATIVE_GL_TESTS=1 ./build-opengl-module/tests-all-opengl \
    --gtest_filter=native_opengl.*
)
```

[Native cases](../../projects/modules/graphics/opengl/tests/acceptance/src/native-opengl.cpp)
cover resource retirement, context restoration, retained-frame reload, failed
construction/reflection cleanup, texture row conventions, timing/backpressure and
composed runtime failure cleanup. Include the replacement-poll/platform-drain
backpressure check and startup/shutdown failures. Accepted uploads settle before game
cleanup, and a later deinit error must preserve the original failure. Resource
retirement or abandonment precedes context destruction; driver deletion queries run
while the borrowed window is still alive.
Do not infer these guarantees from mock tests or an aggregate whose opt-ins are unknown.

The ordered X11 input case additionally needs a Linux/X11 build and session. Native
cases skip unless requested; a requested unusable display does not count as a pass.
If current demo evidence is unavailable, run finite sequential/concurrent sessions
with `--max-updates`; interactive checks are in
[native-desktop-checks.md](native-desktop-checks.md).

[Real-font allocation checks](../../projects/engine/tests/all-tests/src/resources/fonts.cpp)
need `CHERYL_STB_ALLOCATION_TTF` and `CHERYL_STB_ALLOCATION_CFF` to name suitable
TrueType and CFF OpenType faces, each with a glyph wider than 64 pixels at size 128.
DejaVuSans.ttf and NimbusSans-Regular.otf are example fixtures. Export both paths
and select `--gtest_filter=font_bake.real_*` on an Engine acceptance/aggregate or
combined runner. These cases sweep bake scratch allocation failures and nested
scopes, requiring released scratch and no provider upload on failure. Missing
fixture paths skip; unusable supplied paths fail.

[Linux worker checks](../../projects/engine/tests/all-tests/src/core/worker-pool.cpp)
exercise inherited-mask discovery/restoration and native kernel rejection. They can
skip when required CPUs/capabilities are unavailable; controlled policy/thread-start
faults provide different coverage. Identify configuration, opt-ins and skips when
assessing acceptance instead of treating a partial aggregate as complete coverage.

## Manual acceptance drivers

Drivers execute previously built targets and never configure or build:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  python3 projects/engine/tests/logging-acceptance/logging.py build-validation-release
  python3 projects/engine/tests/signal-acceptance/signals.py \
    --release-build build-validation-release --debug-build build-validation-debug
  python3 projects/tests/diagnostics.py build-combined
)
```

The logging procedure and compile-profile matrix are in
[logging-acceptance.md](logging-acceptance.md). Diagnostics require the combined
runner because shader reflection belongs to OpenGL; use the developer logging
profile unless explicitly checking stripped diagnostics. Neither driver needs a
fresh full-aggregate run.

For the Debug signal check, build only `acceptance-signal` against a current
neutral Debug Engine archive and verify its final link excludes native/OpenGL owners.
Configure an Engine-only Debug directory if no suitable archive exists. The POSIX
driver accepts either build argument alone, checks actual `NDEBUG` scope and bounds
children to 15 seconds with core files disabled. Debug requires Backward's trace
header and a stack frame; `NDEBUG` requires ordinary signal termination without
that header. Sanitizer output alone is insufficient. This proves automatic Engine
bootstrap delivery, separately from exception traces or standalone Backward tests;
Windows crash-hook acceptance requires separate checks.

## Evidence limits

Software-driver acceptance does not establish physical GPU/compositor behavior,
hardware reset recovery, physical-device fidelity, arbitrary OS layouts/IME, every
font/transform or exhaustive interleavings. OpenGL 3.3 baseline, full Wayland and
other operating-system support need their own acceptance. Live privileged policy
changes and actual kernel rejection differ from controlled failure injection.
Sanitizers supplement selected checks; TSan requires a configuration without the
Debug ASan/UBSan combination and cannot be inferred from those runs.

Keep source completion distinct from executable acceptance and unavailable-host
coverage. Unresolved work remains in the [planning catalogue](../planning/README.md);
this guide is a reusable procedure, not an execution journal.
