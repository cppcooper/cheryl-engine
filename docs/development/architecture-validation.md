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

Builds and tests require explicit authorization under `AGENTS.md`. Initialize pinned
dependencies with `git submodule update --init --recursive`. OpenGL's GLAD generator
needs Jinja2 in the Python interpreter selected by CMake; set `Python_EXECUTABLE` if
another interpreter supplies it. Native Linux checks need the selected GLFW/X11
platform dependencies; OpenGL checks additionally need OpenGL development libraries.
CMake 4 hosts can require `CMAKE_POLICY_VERSION_MINIMUM=3.5` for pinned dependencies.
Compiler-discovery recovery is documented in
[consuming the engine](consuming-engine.md#standard-headers-in-an-existing-build).

Reuse current execution evidence and compiled targets when their source, options
and link closure still match the required selection. An existing combined build
cannot prove Engine-only isolation. Use fresh directories when an independent graph
is needed, and inspect target dependencies, compilation inventories and final link
commands before execution. A stale source inventory, unselected check or missing
prerequisite is not executable acceptance.

Combine needed build targets into as few invocations as practical. Use low priority
and one job when further builds are necessary, with breaks between them. Run tests
sharing files/logs serially in isolated working directories. A compiler cache can
reuse identical compilation across selections. `CMAKE_CXX_SCAN_FOR_MODULES=OFF`
avoids unnecessary language-module scanning: Cheryl's integration modules are
ordinary libraries. Keep GoogleTest discovery at `PRE_TEST` so building a runner
does not execute its cases. Do not repeat passing checks unless a relevant change
or failure invalidates their evidence.

### Engine only

A fresh Release configuration provides a neutral graph without sandbox mode:

```sh
cmake -S . -B build-validation-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
  -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
  -DCHERYL_LOG_PROFILE=developer \
  -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
  -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF \
  -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
  -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF \
  -DCHERYL_BUILD_DEMO=OFF -DCHERYL_SANDBOX_BUILD=OFF
nice -n 19 cmake --build build-validation-release --parallel 1 --target \
  cheryl-consumer engine-tests logging-tests \
  cheryl-logging-acceptance cheryl-signal-acceptance
./build-validation-release/cheryl-consumer
./build-validation-release/tests-engine
```

Consumers depend on their first-include probes; no separate probe build is needed.
Use the logging and Release signal drivers below. The logging driver also executes
focused logging cases, so they need no additional unit run.

### Native, OpenGL and standalone composition

The [standalone entry points](modules.md#standalone-modules) can prove both the
selected dependency graphs and module bootstrap without building each assembly
twice. Use the same Release, logging and discovery settings as above. Enable
`CHERYL_BUILD_TESTS`, `CHERYL_BUILD_CONSUMER_TESTS` and `CHERYL_BUILD_ALL_TESTS`
for the selected module. Build its targets together:

| Entry point | Targets |
| --- | --- |
| `projects/modules/platform/native-glfw/` | `cheryl-native-glfw-consumer`, `native-glfw-tests`, `native-glfw-all` |
| `projects/modules/graphics/opengl/` | `cheryl-opengl-consumer`, `opengl-tests`, `opengl-acceptance`, `opengl-all` |

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

When combined-assembly evidence is missing, select `CHERYL_BUILD_ALL_TESTS=ON`
in the root build and use its `all-tests` runner. Current combined/demo evidence
can supply that coverage; run only missing owner, native or fixture checks rather
than rebuilding the full assembly solely to repeat it.

## Native and fixture checks

Native OpenGL cases require `CHERYL_NATIVE_GL_TESTS=1` and a usable display:

```sh
CHERYL_NATIVE_GL_TESTS=1 ./build-opengl-module/tests-all-opengl \
  --gtest_filter=native_opengl.*
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
python3 projects/engine/tests/logging-acceptance/logging.py build-validation-release
python3 projects/engine/tests/signal-acceptance/signals.py \
  --release-build build-validation-release --debug-build build-validation-debug
python3 projects/tests/diagnostics.py build-combined
```

The logging procedure and compile-profile matrix are in
[logging-acceptance.md](logging-acceptance.md). Diagnostics require the combined
runner because shader reflection belongs to OpenGL; use the developer logging
profile unless explicitly checking stripped diagnostics. Neither driver needs a
fresh full-aggregate run.

For the Debug signal check, build only `cheryl-signal-acceptance` against a current
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
coverage. Existing feature/resource work remains in [todo.md](../planning/todo.md);
this guide is a reusable procedure, not an execution journal.
