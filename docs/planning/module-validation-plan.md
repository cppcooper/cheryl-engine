# Module extraction validation

## Purpose

This is the live G5 checklist and execution recipe for the extracted Engine / Native
GLFW / OpenGL graph. Checked preparation does not establish executable acceptance.
Existing coverage and host limits are described in
[architecture-validation.md](../development/architecture-validation.md); they do not
replace the checks below. The reported extracted aggregate run predates the
owner-runner composition changes and leaves configuration/native opt-ins unspecified.

Configuration, compilation and executable tests require the explicit authorization in
`AGENTS.md`. Do not treat preparation or historical pre-extraction runs as acceptance
of the current graph.

## Current task — G5

Keep this checklist while G5 is open. Run the executable selections in their listed
order so neutral work can be reused without repeatedly rebuilding unrelated targets:

- [ ] **G5 — establish executable isolation and composition acceptance.**
  - [x] Prepare owner consumers/header probes and owner/cross-project test assemblies;
    source/static review is complete.
  - [x] Prepare the Engine-only signal consumer and bounded manual POSIX driver;
    source/syntax review is complete, with execution pending.
  - [ ] **1. Engine only** — no Cheryl OpenGL/GLAD/GLFW/Gainput/native X11 discovery,
    compilation or linkage. Build the independent consumer/neutral-header probes,
    cheap engine unit runner, logging acceptance and Engine-linked signal consumer.
    Measure the unit suite's build/run cost without native modules or display/device
    requirements.
  - [ ] **2. Engine + Native GLFW** — OpenGL off. Build/run the native consumer/probes
    and native implementation checks; verify Cheryl OpenGL/GLAD remain unselected.
  - [ ] **3. Engine + Native GLFW + OpenGL** — build the OpenGL consumer/probes, module
    implementation checks, owner aggregates, cross-project `all-tests` and demo.
    Native/display opt-ins are recorded explicitly.
  - [ ] **4. Standalone/supplied-target composition** — prove each module reuses one
    Engine, OpenGL reuses one Native GLFW, and dependency bootstrap does not
    recursively enable root modules/tests/demo.
  - [ ] **5. Debug crash bootstrap** — independently prove the Engine target's automatic
    non-`NDEBUG` signal bootstrap with an Engine-only consumer that references no
    engine entry point.
  - [ ] **6. Reconcile G5/U9 status and current contracts** from actual results, resolving
    source failures and identifying any remaining host/display coverage limits.

Inspect the configured target/dependency graph and compilation inventory before
changing each selection. An unselected check or unavailable display is not a pass.

## Release validation configuration

Use a fresh Release directory rather than existing CLion-generated caches. Keep test
discovery at `PRE_TEST` so building a runner does not execute it.

```sh
cmake -S . -B build-validation-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST \
  -DCHERYL_LOG_PROFILE=developer \
  -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_OPENGL=OFF \
  -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON \
  -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF \
  -DCHERYL_BUILD_DEMO=OFF -DCHERYL_SANDBOX_BUILD=OFF

nice -n 19 cmake --build build-validation-release --parallel 1 --target \
  cheryl-consumer cheryl-engine-tests cheryl-logging-tests \
  cheryl-logging-acceptance cheryl-signal-acceptance
```

For Engine-only acceptance, verify the configured/compiled graph itself lacks the
Cheryl native/graphics module targets and sources; merely not executing them is
insufficient. Run the Engine consumer and focused Engine cases, then the manual
logging and Release signal drivers. Consumers depend on their owner header probes;
those probes need no separate build invocation.

Add Native GLFW in the same validation directory, retaining other settings and
explicitly selecting `CHERYL_BUILD_NATIVE_GLFW=ON`, `CHERYL_BUILD_OPENGL=OFF`,
`CHERYL_NATIVE_INPUT=ON`, `CHERYL_NATIVE_NULL_PLATFORM=OFF`, `GLFW_BUILD_X11=ON`
and `GLFW_BUILD_WAYLAND=OFF`. Build these targets together:

```text
cheryl-native-glfw-consumer
cheryl-native-glfw-tests
platform-module_native-glfw-all-tests
```

Run the native consumer and focused native cases. Verify no Cheryl OpenGL backend,
GLAD generation or explicit OpenGL package requirement is selected; upstream GLFW
retains its own context machinery.

Then set `CHERYL_BUILD_OPENGL=ON`, `CHERYL_BUILD_DEMO=ON` and
`CHERYL_BUILD_ALL_TESTS=ON`, retain the native settings and build these together:

```text
cheryl-opengl-consumer
cheryl-opengl-tests
cheryl-opengl-acceptance
engine-all-tests
platform-module_native-glfw-all-tests
graphics-module_opengl-all-tests
all-tests
demo
```

Run the OpenGL consumer and selected combined aggregate; verify each owner runner's
case registration/link contract. Use focused CTest prefixes or direct runners so
the same reusable cases are not repeatedly executed through every aggregate. The
logging driver already executes focused logging cases.

Native OpenGL checks require `CHERYL_NATIVE_GL_TESTS=1` and a usable display. Include
the native timing/publication backpressure case in that run, plus existing native
startup-failure and concurrent-shutdown coverage. Check finite demo runs in both
modes with `--max-updates`; the desktop procedure is in
[native-desktop-checks.md](../development/native-desktop-checks.md). Identify native,
font-fixture and host-dependent skips instead of treating a partial aggregate as
complete acceptance.

## Manual acceptance drivers

After their targets are built, use the existing manual drivers:

```sh
python3 projects/engine/tests/logging-acceptance/logging.py build-validation-release
python3 projects/engine/tests/signal-acceptance/signals.py \
  --release-build build-validation-release
python3 projects/tests/diagnostics.py build-validation-release
```

Run diagnostics after the full selected aggregate is available because shader
reflection belongs to OpenGL. Preserve the developer logging profile unless the
specific stripped-profile mode is being tested.

For the Debug crash check, configure a separate fresh Engine-only Debug directory,
build only `cheryl-signal-acceptance`, then run:

```sh
python3 projects/engine/tests/signal-acceptance/signals.py \
  --debug-build build-validation-debug
```

The driver checks the consumer's actual `NDEBUG` scope: Debug requires Backward's
trace header and a stack frame, while Release requires ordinary signal termination
without that header. Sanitizer output alone is not equivalent evidence. Core files
remain disabled and child execution bounded. These are Engine-linked POSIX checks;
the standalone Backward dependency check does not prove automatic Engine delivery,
and Windows crash-hook acceptance remains separate.

## Standalone modules

Use the standalone module entry points documented in
[modules.md](../development/modules.md). Native GLFW must work against an explicit
Engine checkout/target. OpenGL must work against explicit/reused Engine and Native
GLFW owners. Set `CHERYL_BUILD_TESTS=ON`, `CHERYL_BUILD_CONSUMER_TESTS=ON` and
`CHERYL_BUILD_ALL_TESTS=ON` for its local runners/probes and owner aggregate.
Build its consumer, focused unit runner and owner aggregate together. Verify one
Engine target, no Engine test runners and module-local CTest registration. OpenGL's
Native bootstrap must not select Native tests/consumers recursively.

For supplied-target reuse, an independent host should compose one Engine and both
modules without recursive root selection:

```cmake
cmake_minimum_required(VERSION 3.28)
project(cheryl_module_reuse LANGUAGES C CXX)
enable_testing()

set(CHERYL_BUILD_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
block(SCOPE_FOR VARIABLES)
    set(CHERYL_BUILD_NATIVE_GLFW OFF)
    set(CHERYL_BUILD_OPENGL OFF)
    set(CHERYL_BUILD_TESTS OFF)
    set(CHERYL_BUILD_CONSUMER_TESTS OFF)
    set(CHERYL_BUILD_DEMO OFF)
    add_subdirectory("${CHERYL_ENGINE_SOURCE}" engine EXCLUDE_FROM_ALL)
endblock()

set(CHERYL_BUILD_TESTS ON)
set(CHERYL_BUILD_CONSUMER_TESTS ON)
set(CHERYL_BUILD_ALL_TESTS ON)
add_subdirectory("${CHERYL_NATIVE_GLFW_SOURCE}" native-glfw)
add_subdirectory("${CHERYL_OPENGL_SOURCE}" opengl)
```

Build the two module consumers, focused unit runners and owner aggregates together.
Confirm both modules reuse the supplied Engine and OpenGL reuses the supplied Native
GLFW without extra dependency bootstrap directories or duplicate targets. Use the
same Release/logging/discovery settings as above and explicit source paths; the host
owns root `enable_testing()` policy. Confirm module-local CTest registration and
actual consumer links. This proves source-target composition only; installed/imported
package support is separate work.

## Evidence and execution rules

Run serially and combine requested build targets into as few invocations as practical;
use low priority/one build job as shown above. Tests or drivers sharing files/logs run
in isolated working directories and serially. Do not repeat a passing check unless a
later change or failure makes the evidence stale.

A failure in target ownership, SDK isolation, transitive usage requirements, bootstrap
delivery or standalone composition is G5 work and must be resolved before dependent
U9 assumptions rely on that behavior. Host/display limitations are recorded separately
from source failures.

Update the G5 checklist as subtasks complete, retaining concise checked subtasks
while G5 is open. Update the main roadmap when its acceptance gates are satisfied.
Revise [modules.md](../development/modules.md) only for resulting contracts or enduring
constraints, without adding routine successful execution reports. When G5 is complete,
remove its macro checklist as a whole. Preserve any reusable validation procedure in
the relevant development guide and repair inbound links before deleting this plan.
