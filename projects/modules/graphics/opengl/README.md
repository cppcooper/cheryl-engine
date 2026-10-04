# OpenGL graphics module

`Cheryl::OpenGL` supplies rendering, presentation and graphics resources for the
engine's neutral contracts. It owns the entire OpenGL implementation, generated
GLAD, context operations and the GLFW/OpenGL assembly factory.

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

`tests/unit/` owns mock GL implementation checks. `tests/acceptance/` owns opt-in
native graphics checks, and `tests/consumer/` owns link/header probes. These maps
describe implementation ownership; executable extraction acceptance remains pending.

Target selection, standalone paths and the shared-engine dependency are in
[the module guide](../../../../docs/development/modules.md).
