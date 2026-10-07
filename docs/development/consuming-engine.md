# Consuming the engine

The supported packaging boundary is build-tree composition from a Cheryl checkout.
`projects/engine/` owns the neutral `Cheryl::Engine` target, backed by the build
target `cengine` and archive output name `cheryl-engine`. Applications link selected
integration targets explicitly; [the module guide](modules.md) describes ownership,
standalone composition and tests.

```cmake
set(CHERYL_BUILD_TESTS OFF)
set(CHERYL_BUILD_DEMO OFF)
set(CHERYL_BUILD_NATIVE_GLFW OFF)
set(CHERYL_BUILD_OPENGL OFF)
add_subdirectory(path/to/cheryl-engine cheryl)
add_executable(application main.cpp)
target_link_libraries(application PRIVATE Cheryl::Engine)
```

For native graphics, enable both modules and link `Cheryl::Engine`,
`Cheryl::NativeGLFW` and `Cheryl::OpenGL`. This is the migration from the former
combined engine archive. Consumers obtain C++23, includes, logging policy and SDK
usage requirements through those targets; they do not compile engine sources or
repeat another owner's include/dependency list.

| Requirement | Target scope and reason |
| --- | --- |
| C++23, Threads, logging configuration | Engine public requirements used by templates/runtime consumers. |
| spdlog, CTTI, GLM | Engine public requirements; legacy `glm.hpp` spelling remains supported. |
| Backward::Interface | Engine public trace/resolver requirements; its own global signal-handler object is not linked. |
| STB and JSON | Engine private implementation include directories. |
| FreeType, HarfBuzz, ICU uc/i18n | Engine private font/layout implementation and link requirements; no library-native types appear in public headers. |
| GLFW | Native GLFW private implementation requirement, also used privately by the OpenGL context binding. |
| Gainput | Native GLFW public requirement when `CHERYL_NATIVE_INPUT` is enabled; its types occur in that owner's headers. |
| X11 | Native input's Linux dependency; OpenGL acceptance also uses it for explicitly selected X11 scenarios. |
| GLAD and OpenGL::GL | OpenGL public generated types and private system link requirement. |
| GoogleTest | Test-only; disabled owners do not discover their integration SDKs. |

Engine type introspection uses CTTI 1.1's `name_of` and `type_id_of` APIs.
Singleton diagnostics and `TYPENAME`/`TYPENAMEOF` keep fully qualified names as
`std::string_view`; the expression-name macro preserves `decltype` qualifiers and
does not evaluate its argument. Value type IDs apply decay; explicit type IDs
preserve the supplied type.

The Engine umbrellas and granular contracts are neutral. Concrete window/input
headers belong to Native GLFW; `backends/opengl` headers belong to OpenGL. Existing
`cheryl/...` and legacy `core/...`/`assets/...` include spellings are preserved through
each target's public roots. Installed/exported `find_package` distribution remains
separate work.

## Crash and exception traces

`Cheryl::Engine` delivers the automatic crash bootstrap.
Final consumers receive its object directly, so a static linker cannot omit an
unreferenced archive initializer. Installation follows the bootstrap object's
`NDEBUG` scope in the engine build: Debug-style builds install
`backward::SignalHandling`; `NDEBUG` builds do not.
Exception and explicit stack capture remain available through the existing bounded
capture/fallback implementation in all builds. The legacy global trace resolver and
`Cheryl::SignalHandlers` target remain available; the demo needs only the selected
engine/module targets. See [the module guide](modules.md#crash-and-exception-traces).

## Independent consumers and header probes

[Engine consumer](../../projects/engine/tests/consumer/CMakeLists.txt) links only
Engine and exercises multiple translation units, events, workers and CPU resources.
Its first-include probes include the neutral umbrellas and clipping contract and reject GL/GLFW
header leakage. Standalone bootstrapping selects Engine only in a local scope.

[Native GLFW consumer](../../projects/modules/platform/native-glfw/tests/consumer/CMakeLists.txt)
and [OpenGL consumer](../../projects/modules/graphics/opengl/tests/consumer/CMakeLists.txt)
link their actual module with seven/twelve first-include probes respectively. They
reference real implementation symbols without requiring a display at execution.
`CHERYL_BUILD_CONSUMER_TESTS=ON` adds consumers for the selected root assembly.

After explicit build/test authorization, the engine-only consumer entry point is:

```sh
cmake -S projects/engine/tests/consumer -B build-consumer-engine -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCHERYL_REPOSITORY_ROOT="$PWD"
cmake --build build-consumer-engine --target consumer-cengine --parallel
./build-consumer-engine/cheryl-consumer
```

Standalone consumer entry points require
`CHERYL_REPOSITORY_ROOT=/path/to/cheryl-engine` for shared helpers and bootstrapping.
Engine, Native GLFW and OpenGL consumer bootstraps suppress both UI selections in
a local variable scope, including inherited `ON` choices, without changing the
enclosing configuration's cache.
Normal Native GLFW consumption selects no Cheryl OpenGL/GLAD dependency; the
OpenGL consumer selects both integration owners. CMake 4 hosts may need
`CMAKE_POLICY_VERSION_MINIMUM=3.5` for pinned legacy dependency projects. Standalone
module entry points and explicit null-platform selection are documented separately
in the module guide. [Architecture validation](architecture-validation.md) describes
independent graph, consumer and runner checks and their coverage limits.

## Standard headers in an existing build

An existing CLion profile can retain failed C++ compiler ABI discovery. In the
reported configuration, CMake's generated `CMakeCXXCompiler.cmake` recorded
`CMAKE_CXX_ABI_COMPILED FALSE` and empty implicit include directories. This caused
the X11 dependency's `/usr/include` to become an explicit `-isystem` argument;
GCC 16 then failed to resolve `math.h` or `stdlib.h` through `#include_next` in its
C++ standard headers.

Refresh compiler discovery and regenerate the build files while preserving the
profile/toolchain settings. CMake normally filters its
[detected implicit include directories](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_IMPLICIT_INCLUDE_DIRECTORIES.html)
from explicit compiler arguments. Reload CMake in CLion after repairing an existing
build externally. The recorded recovery refreshed only generated C++ compiler
metadata, preserving cache options and compiled objects; a full cache reset must
retain the intended configuration options too.
