# Consuming the engine

The supported U7 packaging boundary is **build-tree composition from this checkout**.
Add Cheryl as a subdirectory and link Cheryl::Engine. The original cherylGL target
remains available. C++23, public header directories, logging policy, and transitive
dependencies belong to the target; applications do not compile engine sources or
repeat the demo's dependency list.

The repository root remains the CMake entry point. `projects/engine/CMakeLists.txt`
defines the engine and publishes its public include roots from
`projects/engine/include/`; consumers obtain them through the target. The directory
migration preserves the combined engine's native/OpenGL link contract. The later
module extraction has its own migration gate in the
[groundwork plan](../planning/module-groundwork-and-extraction-plan.md).

```cmake
set(CHERYL_BUILD_TESTS OFF CACHE BOOL "Build Cheryl tests")
set(CHERYL_BUILD_DEMO OFF CACHE BOOL "Build Cheryl demo")
add_subdirectory(path/to/cheryl-engine cheryl)
add_executable(application main.cpp)
target_link_libraries(application PRIVATE Cheryl::Engine)
```

Select CHERYL_SANDBOX_BUILD and target-wide logging settings before adding Cheryl.
Normal builds include the GLFW/Gainput native input adapter. Sandbox builds exclude
it and its X11/Gainput requirements; generic input/runtime contracts and the GLFW
null platform remain. Sandbox is not a backend-free SDK. GLFW, GLAD and OpenGL remain
part of the current engine target. No UI toolkit dependency is selected.

| Requirement | Target scope and reason |
| --- | --- |
| C++23, Threads, logging configuration | Public: templates and runtime consumers use these contracts. |
| spdlog, CTTI, GLM | Public: exported logging/singleton/render/resource headers contain their types/includes. The existing glm.hpp spelling is supported. |
| Backward::Interface | Public: diagnostic headers need its include/configuration and resolver libraries. It does not add a signal-handling object. |
| GLAD | Public: explicitly selected OpenGL headers expose generated types; generic headers remain free of GL/GLFW includes. |
| GLFW and OpenGL::GL | Private implementation dependencies; static consumers receive required final link dependencies. |
| Gainput, normal configuration | Public: InputSystem/InputMapper expose its types. Its missing include propagation is supplied by Cheryl. |
| X11, normal Unix/Linux configuration | Private native link requirement, excluded in sandbox. |
| STB and JSON | Private implementation include directories. |
| GoogleTest | Test-only; excluded when CHERYL_BUILD_TESTS is OFF. |

Prefer granular headers such as cheryl/core/engine/engine-context.h,
cheryl/core/controls/input-interface.h, cheryl/core/rendering/render-frame.h, and
cheryl/assets/resources/resource-provider.h for backend-neutral consumers. Native
InputSystem and backends/opengl headers deliberately select native dependencies.
The core.h umbrella includes the normal native input adapter and is not a guarantee
of a backend-neutral header surface. Both root-qualified cheryl/... includes and
the repository's legacy core/.../assets/... spelling are propagated.

No install/export/find_package distribution is promised by this unit. Vendored
dependency exports, the legacy public include spelling, backend selection, and
redistribution need a separate packaging unit before an installed package is
advertised. Linux normal/sandbox acceptance does not establish Windows/macOS or
full Wayland support; their native dependency/link paths need separate evidence.

## Application bootstrap

The engine archive has no automatic process signal-handler bootstrap. Stack
capture resolves local traces through Backward::Interface. Backward's Object and
Backward library targets contain a global signal handler and are not linked into
the engine target merely to resolve traces.

An application that explicitly wants the repository's legacy global sh/tr objects
may link Cheryl::SignalHandlers. That object installs backward::SignalHandling
before main; the demo opts in. Embedded hosts should own any signal policy directly
and may create their own scoped Backward handlers. Do not combine independent
global handler owners without an application-level ordering policy.

## Independent acceptance

[engine consumer](../../projects/engine/tests/consumer/CMakeLists.txt) is a separate CMake application.
It links only Cheryl::Engine, uses multiple translation units, exercises event,
worker and CPU-backed provider symbols, and compiles selected public headers as
first includes. It does not set include directories or a language standard and
does not require a window/display at execution. Its generic translation units
reject accidental OpenGL/GLFW header exposure.

After focused build/test authorization:

```sh
cmake -S projects/engine/tests/consumer -B build-consumer-sandbox \
  -DCMAKE_BUILD_TYPE=Release -DCHERYL_SANDBOX_BUILD=ON
cmake --build build-consumer-sandbox --target cheryl-consumer --parallel 3
./build-consumer-sandbox/cheryl-consumer
```

Repeat normal Linux with GLFW_BUILD_X11=ON and GLFW_BUILD_WAYLAND=OFF. CMake 4 hosts
may need CMAKE_POLICY_VERSION_MINIMUM=3.5 for the pinned legacy dependency projects;
this compatibility setting does not upgrade their policy declarations. Executable
consumer/header results must be recorded separately from aggregate tests.

To reuse an existing root build, configure it with `CHERYL_BUILD_CONSUMER_TESTS=ON`,
then build `cheryl-consumer` with `--parallel 1` and run it from that build directory.
This opt-in adds the same consumer and header probes after `Cheryl::Engine` exists;
it reuses the engine archive and does not require the aggregate test suite.
