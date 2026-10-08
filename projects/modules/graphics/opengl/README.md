# OpenGL graphics module

`Cheryl::OpenGL` supplies rendering, presentation and graphics resources for the
engine's neutral contracts. It owns the entire OpenGL implementation, generated
GLAD, context operations and the GLFW/OpenGL assembly factory.

## Selection and composition

The root selects this owner with `CHERYL_BUILD_OPENGL=ON`, which requires
`CHERYL_BUILD_NATIVE_GLFW=ON`. Applications link `Cheryl::OpenGL` to inherit its
Engine, Native GLFW and generated GLAD requirements. Its build target is
`module_opengl`, with output name `cheryl-module-opengl`:

```cmake
target_link_libraries(game PRIVATE Cheryl::OpenGL)
```

From the repository root, a standalone build reuses supplied Engine/Native GLFW
targets or bootstraps the explicitly selected owners:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S projects/modules/graphics/opengl -B build/opengl-module -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCHERYL_REPOSITORY_ROOT="$PWD" \
    -DCHERYL_NATIVE_GLFW_SOURCE="$PWD/projects/modules/platform/native-glfw"
)
```

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake --build build/opengl-module --target module_opengl --parallel
)
```

System OpenGL development files and Python/Jinja2 for GLAD are required. Native
window/input dependencies follow the selected Native GLFW configuration; see the
[dependency table](../../../../docs/development/building.md#system-dependencies).

## Command-line startup

Link `Cheryl::OpenGL::Startup` and include `cheryl/backends/opengl/startup.h` for
`CE::Engine::make_glfw_opengl_startup`. This separate support library has build
target `module_opengl_startup` and output name `cheryl-module-opengl-startup`.
It links `Cheryl::Startup` and `Cheryl::OpenGL`; the ordinary OpenGL target does
not link back to startup. The
[application guide](../../../../docs/development/consuming-engine.md#command-line-startup)
owns common parsing, configuration and result-lifetime semantics.

The default-input overload takes a description, `GlfwOpenGLConfig` and
`RuntimeConfiguration`, each with a default, and is available with
`CHERYL_NATIVE_INPUT`. The explicit-input overload takes `iInputSystem&` first
and borrows it through context teardown. Both use the existing
`make_glfw_opengl_context` assembly factory after validation.

| Backend argument | Effect |
| --- | --- |
| `--window-width=N` | Sets positive logical window width; the factory default is 1280. |
| `--window-height=N` | Sets positive logical window height; the factory default is 720. |
| `--window-title=TEXT` | Overrides the title; an empty configured title keeps the randomized title behavior. |
| `--swap-interval=N` | Sets a nonnegative presentation swap interval; the factory default is 1. |
| `--input-diagnostics` | Available only with the default owned native input. Requires compiled TRACE logging and enables controller diagnostics in the OS-platform logger. |

Help and invalid configuration return before GLFW/window construction.
`--input-diagnostics` sets the OS-platform file/logger gates to TRACE while
retaining the console preset, then enables the owned input adapter's gamepad
diagnostics. It is absent from the borrowed-input overload. The
[native diagnostics guide](../../platform/native-glfw/README.md#controller-diagnostics)
describes those records and their limits.

## Engine contracts

| Engine contract | Implementation | Responsibility |
| --- | --- | --- |
| [`CE::RenderAPIs::iRenderer`](../../../engine/include/cheryl/core/rendering/renderer.h) | [`OpenGLRenderer`](include/cheryl/backends/opengl/renderer.h) | Consume published render frames and maintain graphics resources on their owner. |
| [`CE::RenderAPIs::iPresentationSurface`](../../../engine/include/cheryl/core/rendering/presentation-surface.h) | [`GlfwOpenGLContext`](include/cheryl/backends/opengl/glfw-context.h), through [`iOpenGLContext`](include/cheryl/backends/opengl/context.h) | Present a completed frame using a borrowed GLFW window and its OpenGL context. |
| [`CE::Assets::ResourceProvider`](../../../engine/include/cheryl/assets/resources/resource-provider.h) | [`OpenGLResourceProvider`](include/cheryl/backends/opengl/resource-provider.h) | Create images, font atlases, geometry and linked programs in the renderer's domain. |
| [`CE::Assets::Image`](../../../engine/include/cheryl/assets/resources/image.h) | [`Texture`](include/cheryl/backends/opengl/texture.h) | Immutable uploaded pixel dimensions and texture binding. |
| [`CE::Assets::Geometry2D`](../../../engine/include/cheryl/assets/resources/geometry2d.h) | [`VAO`](include/cheryl/backends/opengl/vertex-array-object.h) | Uploaded geometry metadata, binding and draw ranges. |
| [`CE::Assets::Shader`](../../../engine/include/cheryl/assets/resources/shader.h) | [`GLSLProgram`](include/cheryl/backends/opengl/glslprogram.h) | Executable shader programs and pass/draw parameter binding. |
| [`CE::Assets::Pipeline`](../../../engine/include/cheryl/assets/resources/pipeline.h) | [`GLSLPipeline`](include/cheryl/backends/opengl/pipeline.h) | Realize the engine's pipeline definition and semantic parameter bindings. |

`iOpenGLContext` extends the engine presentation contract with OpenGL context
selection and entry-point lookup. That extension belongs to this graphics owner.
Its public headers retain the existing `backends/opengl/` spelling; implementation
and private helpers live together under `src/backends/opengl/`.

## Checks

| Target | Output / kind | Selection / coverage |
| --- | --- | --- |
| `tests-opengl` | `tests-opengl` | `CHERYL_BUILD_TESTS`: mock GL checks without a graphics context. |
| `acceptance-opengl` | `tests-acceptance-opengl` | Native graphics/runtime checks when tests and native input are selected. `CHERYL_BUILD_ACCEPTANCE_TESTS` adds it to the default build/CTest; native cases require `CHERYL_NATIVE_GL_TESTS=1` and a usable display. |
| `all-opengl` | `tests-all-opengl` | All selected owner GoogleTests; `CHERYL_BUILD_ALL_TESTS` adds it to the default build/CTest. |
| `consumer-module-opengl` | `cheryl-opengl-consumer` | `CHERYL_BUILD_CONSUMER_TESTS`: independent link/implementation consumer. |
| `consumer-module-headers--opengl` | Object library | Consumer's first-include header probes; built with the consumer. |

Native and fixture opt-ins apply to the aggregate too. Mock checks or skipped
native cases do not establish driver, compositor or physical-device behavior.

Target selection, standalone paths and the shared-engine dependency are in
[the module guide](../../../../docs/development/modules.md) and
[architecture validation](../../../../docs/development/architecture-validation.md).
