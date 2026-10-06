# OpenGL graphics module

`Cheryl::OpenGL` supplies rendering, presentation and graphics resources for the
engine's neutral contracts. It owns the entire OpenGL implementation, generated
GLAD, context operations and the GLFW/OpenGL assembly factory.

## Selection and composition

The root selects this owner with `CHERYL_BUILD_OPENGL=ON`, which requires
`CHERYL_BUILD_NATIVE_GLFW=ON`. Applications link `Cheryl::OpenGL` to inherit its
Engine, Native GLFW and generated GLAD requirements:

```cmake
target_link_libraries(game PRIVATE Cheryl::OpenGL)
```

From the repository root, a standalone build reuses supplied Engine/Native GLFW
targets or bootstraps the explicitly selected owners:

```sh
cmake -S projects/modules/graphics/opengl -B build/opengl-module \
  -DCMAKE_BUILD_TYPE=Release \
  -DCHERYL_ENGINE_SOURCE=/absolute/path/to/cheryl-engine \
  -DCHERYL_NATIVE_GLFW_SOURCE=/absolute/path/to/cheryl-engine/projects/modules/platform/native-glfw
cmake --build build/opengl-module --target module_opengl --parallel 1
```

System OpenGL development files and Python/Jinja2 for GLAD are required. Native
window/input dependencies follow the selected Native GLFW configuration; see the
[dependency table](../../../../README.md#dependencies).

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

| Target / directory | Coverage |
| --- | --- |
| `opengl-tests`, `tests/unit/` | Mock GL implementation checks without a graphics context. |
| `opengl-acceptance`, `tests/acceptance/` | Native graphics/runtime checks when native input is selected. Requires `CHERYL_NATIVE_GL_TESTS=1` and a usable display for native cases. |
| `opengl-all`, `tests/all-tests/` | All selected owner GoogleTests. |
| `cheryl-opengl-consumer`, `tests/consumer/` | Link and first-include header probes selected with `CHERYL_BUILD_CONSUMER_TESTS=ON`. |

Native and fixture opt-ins apply to the aggregate too. Mock checks or skipped
native cases do not establish driver, compositor or physical-device behavior.

Target selection, standalone paths and the shared-engine dependency are in
[the module guide](../../../../docs/development/modules.md) and
[architecture validation](../../../../docs/development/architecture-validation.md).
