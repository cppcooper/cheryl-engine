# Module extraction validation

## Purpose

This is the live G5 checklist and execution recipe for the extracted Engine / Native
GLFW / OpenGL graph. Checked preparation does not establish executable acceptance.
Existing coverage and host limits are described in
[architecture-validation.md](../development/architecture-validation.md); they do not
replace the checks below. The owner reports that the current combined `all-tests`
and demo work. Use that evidence for the combined assembly; native/font opt-ins and
skip identities remain unspecified. Focus further execution on isolated selections,
independent consumers, standalone registration and automatic signal delivery.

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
  - [x] Correct standalone module CTest enablement in directory scope; executable
    registration checks belong to selection 4.
  - [ ] **1. Engine only** — no Cheryl OpenGL/GLAD/GLFW/Gainput/native X11 discovery,
    compilation or linkage. Build the independent consumer/neutral-header probes,
    cheap engine unit runner, logging acceptance and Engine-linked signal consumer.
    Measure the unit suite's build/run cost without native modules or display/device
    requirements.
  - [ ] **2. Engine + Native GLFW** — OpenGL off. Build/run the native consumer/probes
    and native implementation checks; verify Cheryl OpenGL/GLAD remain unselected.
  - [ ] **3. Engine + Native GLFW + OpenGL** — prove consumer/probe links and owner
    registrations. Reuse the owner's current combined `all-tests`/demo evidence;
    run only missing native/display opt-ins, recording their coverage explicitly.
  - [ ] **4. Standalone/supplied-target composition** — prove each module reuses one
    Engine, OpenGL reuses one Native GLFW, and dependency bootstrap does not
    recursively enable root modules/tests/demo.
  - [ ] **5. Debug crash bootstrap** — independently prove the Engine target's automatic
    non-`NDEBUG` signal bootstrap with an Engine-only consumer that references no
    engine entry point. Reuse the existing Debug Engine archive after checking its
    current target/link closure; a fresh Debug build is needed only if it is stale.
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
  cheryl-consumer engine-tests logging-tests \
  cheryl-logging-acceptance cheryl-signal-acceptance
```

For Engine-only acceptance, verify the configured/compiled graph itself lacks the
Cheryl native/graphics module targets and sources; merely not executing them is
insufficient. Run the Engine consumer and focused Engine cases, then the manual
logging and Release signal drivers. Consumers depend on their owner header probes;
those probes need no separate build invocation.

For Native GLFW, retain the same Release/compiler-cache settings and
explicitly select `CHERYL_BUILD_NATIVE_GLFW=ON`, `CHERYL_BUILD_OPENGL=OFF`,
`CHERYL_NATIVE_INPUT=ON`, `CHERYL_NATIVE_NULL_PLATFORM=OFF`, `GLFW_BUILD_X11=ON`
and `GLFW_BUILD_WAYLAND=OFF`. Build these targets together:

```text
cheryl-native-glfw-consumer
native-glfw-tests
native-glfw-all
```

Run the native consumer and focused native cases. Verify no Cheryl OpenGL backend,
GLAD generation or explicit OpenGL package requirement is selected; upstream GLFW
retains its own context machinery.

For OpenGL, retain the native settings and build its consumer and owner runners
together:

```text
cheryl-opengl-consumer
opengl-tests
opengl-acceptance
opengl-all
```

Run the OpenGL consumer and owner cases; verify runner registration/link contracts.
Use focused CTest prefixes or direct runners so reusable cases execute only once.
The logging driver already executes focused logging cases.

When the current combined assembly already has owner execution evidence, avoid
rebuilding/rerunning it solely for this gate. Reuse its compiled case objects for
owner registration checks and targeted native acceptance. Build independent
consumers separately; isolated Engine/Native and standalone graphs still need their
own configuration/link evidence. A task-local compiler cache may reuse identical
compilations between those directories. Disable C++ language module scanning in
these validation builds: Cheryl's integration modules are ordinary libraries, and
the source tree declares no C++ language modules.

Native and OpenGL standalone entry points can serve selections 2 and 3 as well as
their bootstrap checks in selection 4. This avoids building each assembly twice.
Keep tests/consumers enabled for the selected owner and verify its bootstrapped
Engine/Native dependencies have their own tests disabled. Reuse the current combined
build's case objects for inexpensive owner-runner registration checks.

Native OpenGL checks require `CHERYL_NATIVE_GL_TESTS=1` and a usable display. Include
the native timing/publication backpressure case in that run, plus existing native
startup-failure and concurrent-shutdown coverage. An already verified current demo
does not need another smoke run. When that evidence is missing, check finite runs
in both modes with `--max-updates`; the desktop procedure is in
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

Run diagnostics against the current combined build because shader reflection
belongs to OpenGL; it need not rebuild or rerun the whole aggregate. Preserve the
developer logging profile unless the specific stripped-profile mode is being tested.

For the Debug crash check, reuse a current Debug build's neutral Engine target and
build only `cheryl-signal-acceptance`. Inspect its link command to verify no native
or OpenGL module is pulled in. If the available Engine is stale or its Debug scope
is unsuitable, configure a separate Engine-only Debug directory instead. Then run:

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
