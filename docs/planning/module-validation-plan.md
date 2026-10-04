# Module extraction validation

G5 follows the implemented extraction and role-directory organization. The working
tree is clean at the start of this unit. Configuration, compilation and executable
tests require the explicit request specified by AGENTS.md; preparation and static
checks can proceed independently. The owner selected preparation and static checks
only for this pass; no configuration, compilation or executable tests are authorized.

## Owner aggregate follow-up

The owner reports that the extracted `all-tests` target compiled and ran with 393
passing cases and 17 ignored/skipped cases. This accounts for the 410 named cases;
the configuration, skip identities and native opt-ins were not supplied. Record
this as owner-reported aggregate evidence without inferring the remaining G5 results.

The owner requests separate complete runners for each project and a cross-project
test owner under `projects/tests/`. Implement this as a coherent follow-up:

1. Give Engine, Native GLFW and OpenGL their own aggregate under each owner's
   `tests/all-tests/`: `engine-all-tests`, `platform-module_native-glfw-all-tests`,
   and `graphics-module_opengl-all-tests`. Keep existing focused and manual runners.
2. Compile each focused suite's cases as a reusable object library with its own
   private hooks, definitions and dependencies. Separate shared test support from
   the runner entry point so an aggregate has exactly one main. Link the same cases
   directly into focused, owner and cross-project runners without archive extraction
   losing test registration or duplicate case compilation.
3. Create combined `all-tests` only in `projects/tests/`, after selected owners have
   registered their case objects. Move the cross-project diagnostics driver there.
   Keep the executable name/output and existing selection options; propagate native
   acceptance dependencies through its case target, including conditional X11.
4. Statically verify case ownership, exact before/after names and source bytes,
   private hook scoping, conditional selections, one runner entry point and repaired
   paths/docs. Record the new runner layout and owner-reported execution separately.
   Do not configure, compile or run tests in this follow-up without an explicit request.

## Sequence

1. Prepare a small Engine-only signal-acceptance consumer and isolated-process driver.
   Its fatal-signal case links only `Cheryl::Engine` and calls no engine function,
   proving automatic bootstrap delivery despite static archive extraction. Compare
   builds with and without `NDEBUG`; exception/explicit trace checks remain separate.
   Do not use the standalone Backward dependency check as evidence for Engine.
   Implementation: [consumer](../../projects/engine/tests/signal-acceptance/src/main.cpp)
   and [manual POSIX driver](../../projects/engine/tests/signal-acceptance/signals.py).
   Source and syntax review completed; build and execution remain pending.
2. Use a fresh Release validation directory with developer logging and one
   low-priority build job. Preserve the existing CLion profiles, whose caches refer
   to another checkout path. Enable the assemblies in order in this one directory
   so completed neutral/dependency objects can be reused:
   - Engine only: neutral first-include probes, consumer, cheap Engine unit runner,
     logging acceptance and the release signal consumer. Capture source/dependency
     inventories and build/run cost without native/graphics SDKs.
   - Engine + Native GLFW: native consumer/probes and real native mapping/diagnostic
     unit cases. Verify that OpenGL/GLAD are not selected.
   - Full OpenGL: OpenGL consumer/probes, module mock suites, selected aggregate,
     native graphics cases and finite demo runs. Record native opt-ins and skips
     separately; keep the native timing/publication case within the focused run.
3. Validate standalone module bootstrap and supplied-target reuse with explicit
   source paths and locally disabled recursive selections. Check module-local test
   registration, one Engine target and owner source/dependency inventories. Build
   the standalone consumers needed to establish the actual link contract.
4. Use a separate Debug Engine-only configuration for the Engine-linked fatal-signal
   check. Run bounded child processes with core files disabled and keep sanitizer
   output distinct from Backward's trace output. Record the actual `NDEBUG` scope.
5. Fix only failures required for this validation unit, committing coherent fixes
   independently. Update extraction evidence and the G5/U9 status from actual
   results; keep any unexecuted or unsupported-host requirement explicit.

## Static preparation completed

- Production manifests declare each owner source exactly once: Engine 52, Native
  GLFW six, OpenGL eleven. All 408 original named GoogleTest cases remain, with two
  new runtime cases and no duplicate names. The owner-committed memory source is
  unchanged from the extraction baseline.
- Engine production includes contain no concrete module or GLFW/GLAD/Gainput
  imports. Neutral dependency discovery does not select native/graphics SDKs.
  Consumer probes belong to their linked owners: 13 Engine, seven Native GLFW,
  twelve OpenGL.
- Standalone helper paths resolve after the role-directory move. Engine bootstrap
  suppresses recursive module/demo/test/consumer selection in a local scope.
  OpenGL explicitly requires Native GLFW or its source path; supplied targets are
  reused. Module tests attach to their implementing owner and use shared Engine
  test support without enabling the engine's broader acceptance.
- The signal consumer/driver and their CMake entry are prepared. Python AST parsing
  checks syntax without executing the driver. The driver requires actual `NDEBUG`
  scope and Backward-specific trace output, with core files disabled and a bounded
  fatal child. Its target is opt-in for the default build and manual for execution.

These findings establish source-level preparation only. CMake configuration,
transitive linking, actual bootstrap delivery and execution costs remain unproven.

## Prepared executable selections

The commands below are for a later explicitly authorized execution pass. They have
not been run. Use a fresh directory from this checkout; existing CLion Debug and
Release caches name `/home/jcooper/Documents/projects/cheryl-engine` as their source
root. Keep `PRE_TEST` discovery so building a runner does not execute it to enumerate
GoogleTest cases. The compatibility policy value below matches the earlier recorded
CMake 4 runs with the pinned dependencies.

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

Capture the configured target/dependency graph and compilation inventory before
each selection changes. Engine-only must lack native/graphics targets and source
entries, not merely skip their final executables. Save build/run timing per phase.
Consumer targets depend on their owner header probes, so those probes do not need
separate build invocations.

| Phase | Change in the Release directory | Combined build targets | Executable evidence |
| --- | --- | --- | --- |
| Engine only | Initial configuration above. | Targets above. | Engine consumer; focused Engine cases; logging driver; signal driver with `--release-build`. |
| Add Native GLFW | Native ON, OpenGL OFF; native input ON, null platform OFF; GLFW X11 ON and Wayland OFF. | `cheryl-native-glfw-consumer`, `cheryl-native-glfw-tests`. | Native consumer and focused native cases; no GLAD/OpenGL selection. |
| Add OpenGL | OpenGL ON; demo ON; aggregate ON; retain the native choices. | `cheryl-opengl-consumer`, `cheryl-opengl-tests`, `cheryl-opengl-acceptance`, `all-tests`, `demo`. | OpenGL consumer; selected aggregate with native opt-in on a usable display; diagnostics driver; finite demo runs. |

Select focused Engine/native cases through their distinct CTest prefixes and run
serially. The logging driver already runs the focused logging cases. In the full
assembly, the aggregate covers the module mock and native cases; building their
focused runners establishes their separate link contracts without requiring a
second execution of the same cases. Missing native opt-ins or an unusable display
must be recorded rather than counted as passes. Demo instructions are in
[native-desktop-checks.md](../development/native-desktop-checks.md).

The manual drivers consume previously built outputs:

```sh
python3 projects/engine/tests/logging-acceptance/logging.py build-validation-release
python3 projects/engine/tests/signal-acceptance/signals.py --release-build build-validation-release
python3 projects/engine/tests/all-tests/diagnostics.py build-validation-release
```

Run diagnostics only after the full aggregate is available; its shader-reflection
case belongs to OpenGL. Each driver creates isolated working directories. Preserve
the developer logging profile for diagnostics; a stripped profile requires its
explicit `--info-stripped` mode and separate profile evidence.

Use a second fresh directory for Debug Engine only, retaining the Engine-only
selectors above and changing `CMAKE_BUILD_TYPE` to Debug. Build only
`cheryl-signal-acceptance` with one low-priority job, then pass that directory to the
signal driver's `--debug-build` option. No display or module is needed.

## Standalone and supplied-target cases

Standalone Native GLFW and OpenGL entry points are documented in
[the module guide](../development/modules.md#standalone-modules). Enable module-local
tests and consumers explicitly. For Native, build its consumer and focused runner;
for OpenGL, build its consumer and focused mock runner. Each should have one Engine
target, no Engine test runners, and only its own selected tests/consumer probes.
OpenGL's Native dependency bootstrap must not add Native tests/consumers recursively.

For supplied-target reuse, use an independent host with this composition, explicit
source paths, and the same Release/logging/discovery settings as above. The host
enables CTest at its own root; module-local `enable_testing()` does not establish
that host policy.

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
add_subdirectory("${CHERYL_NATIVE_GLFW_SOURCE}" native-glfw)
add_subdirectory("${CHERYL_OPENGL_SOURCE}" opengl)
```

Build the two module consumers and focused unit runners together. Verify that both
modules reuse the supplied Engine, OpenGL reuses the supplied Native GLFW, and no
additional engine/native bootstrap directory or duplicate target is introduced.
Record module-local CTest registration and actual consumer links. This source-target
case does not establish installed/imported package support.

## Execution boundaries

Run serially and combine requested targets into as few build invocations as
practical. Use `nice -n 19` and `--parallel 1` for builds. Tests that share fixtures
or log directories run in isolated working directories and serially. Do not repeat
passed checks unless a change, failure or unresolved concern justifies it.

A failure in source ownership, transitive target usage, bootstrap delivery or
standalone composition is part of G5 and must be resolved before dependent adapter
implementation. A host/display limitation is recorded separately from a source
failure. If a difficult design issue would benefit from deeper reasoning, pause
with the evidence and alternatives for owner review as requested.
