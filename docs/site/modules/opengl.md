(opengl-graphics-module)=
# OpenGL backend reference

`Cheryl::OpenGL` supplies rendering, presentation and graphics resources for the
engine's neutral contracts. It owns the entire OpenGL implementation, generated
GLAD, context operations and the GLFW/OpenGL assembly factory.

## Selection and composition

The root selects this owner with `CHERYL_BUILD_OPENGL=ON`, which requires
`CHERYL_BUILD_NATIVE_GLFW=ON`. Applications link `Cheryl::OpenGL` to inherit its
Engine and Native GLFW requirements. GLAD is private compiled implementation;
public uniform-setting frontends call compiled OpenGL operations rather than
emitting loader calls in application translation units. Its build target is
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
[dependency table](../development/building.md#system-dependencies).

## Command-line startup

Link the public [Startup module](startup.md),
`Cheryl::Startup`, and include `<cheryl/application/startup.h>` for
`CE::Application::make_startup()`. Configured OpenGL argument/default-factory glue
is private to Startup; ordinary OpenGL does not link back to it.
`Cheryl::OpenGL::Startup` and the backend-specific header/factory remain
compatibility interfaces, with no separate OpenGL-startup binary. The
[Source and SDK integration](../development/consuming-engine.md#command-line-startup)
owns common parsing, configuration and result-lifetime semantics.

The generic factory accepts a description and `CE::Application::ApplicationOptions`.
Its explicit-input overload takes `CE::Input::System&` first and borrows it
through context teardown. The standard owned-input path requires native input.
Both use `CE::Application::GLFWOpenGL::make_context()` after validation.

With `CHERYL_NATIVE_INPUT=OFF`, the OpenGL header omits the owned-input
`make_context(options)` overload. Supply an application-owned input system to
the explicit-input overload instead; generic Startup remains usable with that
input.

| Backend argument | Effect |
| --- | --- |
| `--backend=default` or `--backend=opengl` | Selects the included standard backend, not a runtime plugin. |
| `--window-width=N` | Sets positive logical window width; the factory default is 1280. |
| `--window-height=N` | Sets positive logical window height; the factory default is 720. |
| `--window-title=TEXT` | Overrides the title; an empty configured title keeps the randomized title behavior. |
| `--swap-interval=N` | Sets a nonnegative presentation swap interval; the factory default is 1. |
| `--input-diagnostics` | Available only with the default owned native input. Requires compiled TRACE logging and enables controller diagnostics in the OS-platform logger. |

Help and invalid configuration return before GLFW/window construction.
`--input-diagnostics` sets the OS-platform file/logger gates to TRACE while
retaining the console preset, then enables the owned input adapter's gamepad
diagnostics. It is absent from the borrowed-input overload. The
[native diagnostics guide](native-glfw.md#controller-diagnostics)
describes those records and their limits.

## Engine contracts

| Engine contract | Implementation | Responsibility |
| --- | --- | --- |
| [`CE::Rendering::Renderer`](../../../projects/engine/include/cheryl/core/rendering/renderer.h) | [`CE::Rendering::OpenGL::Renderer`](../../../projects/modules/graphics/opengl/include/cheryl/backends/opengl/renderer.h) | Consume published render frames and maintain graphics resources on their owner. |
| [`CE::Rendering::PresentationSurface`](../../../projects/engine/include/cheryl/core/rendering/presentation-surface.h) | [`GLFWContext`](../../../projects/modules/graphics/opengl/include/cheryl/backends/opengl/glfw-context.h), through [`CE::Rendering::OpenGL::Context`](../../../projects/modules/graphics/opengl/include/cheryl/backends/opengl/context.h) | Present a completed frame using a borrowed GLFW window and its OpenGL context. |
| [`CE::Rendering::Resources::Provider`](../../../projects/engine/include/cheryl/assets/resources/resource-provider.h) | [`CE::Rendering::OpenGL::ResourceProvider`](../../../projects/modules/graphics/opengl/include/cheryl/backends/opengl/resource-provider.h) | Create images, font atlases, geometry and linked programs in the renderer's domain. |
| [`CE::Rendering::Resources::Image`](../../../projects/engine/include/cheryl/assets/resources/image.h) | [`Texture`](../../../projects/modules/graphics/opengl/include/cheryl/backends/opengl/texture.h) | Immutable uploaded pixel dimensions and texture binding. |
| [`CE::Rendering::Sampling::Sampler`](../../../projects/engine/include/cheryl/assets/resources/sampler.h) | [`CE::Rendering::OpenGL::Sampler`](../../../projects/modules/graphics/opengl/include/cheryl/backends/opengl/sampler.h) | Immutable per-binding filtering/wrap/anisotropy and deferred native retirement. |
| [`CE::Rendering::Geometry::Buffer2D`](../../../projects/engine/include/cheryl/assets/resources/geometry2d.h) | [`VertexArray`](../../../projects/modules/graphics/opengl/include/cheryl/backends/opengl/vertex-array-object.h) | Uploaded geometry metadata, binding and draw ranges. |
| [`CE::Rendering::Resources::Program`](../../../projects/engine/include/cheryl/assets/resources/shader.h) | [`Program`](../../../projects/modules/graphics/opengl/include/cheryl/backends/opengl/glslprogram.h) | Executable shader programs and pass/draw parameter binding. |
| [`CE::Rendering::Resources::Pipeline`](../../../projects/engine/include/cheryl/assets/resources/pipeline.h) | [`Pipeline`](../../../projects/modules/graphics/opengl/include/cheryl/backends/opengl/pipeline.h) | Realize the engine's pipeline definition and semantic parameter bindings. |

`CE::Rendering::OpenGL::Context` extends the engine presentation contract with OpenGL context
selection and entry-point lookup. That extension belongs to this graphics owner.
Its public headers retain the existing `backends/opengl/` spelling; implementation
and private helpers live together under `src/backends/opengl/`.

Create/upload resources and perform native rendering on that context's owner with
it current. Simulation prepares CPU data and immutable frame submissions; use the
[platform dispatch path](../runtime/thread-dispatch.md) for a later upload. Logical
handle release may occur on another thread, but native deletion is deferred to
platform maintenance. The [residency contract](../resources/resource-residency.md#native-retirement-and-maintenance)
owns retirement, shutdown and late-release details.

The provider also implements optional `CE::Assets::Shaders::Builder` construction for
[indexed recipes](../assets/asset-manifests.md#shadermaterial-definitions),
linking prepared owned source bytes and interpreting only `bindings.opengl`.
The same builder realizes Engine's `Builtin2D` profiles from embedded GLSL and
bindings. Ordinary `Textured`/`TexturedStrip` presets serve uncolored image/glyph
triangles and atlas strips with projection/view/model/scale/alpha semantics.
TGUI and RmlUi use the separate colored-triangle profiles and request their own
alpha state through that generic contract, without linking this module. Programs
are weakly reused within one provider, including ordinary triangle/strip sharing
with the same alpha convention; materials are cached by profile/blend.
Requests still require its live owner/current context on cache hits. See
[default asset materials](../rendering/pipelines-and-materials.md#default-asset-materials)
and [default UI materials](../rendering/pipelines-and-materials.md#default-ui-materials).
Manual pipeline/material builders remain available. Engine image defaults request
maximum-supported anisotropy; explicit samplers preserve independent immutable
sampling for shared textures. See the
[rendering contract](../rendering/pipelines-and-materials.md#immutable-sampling).

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
[the module guide](../development/modules.md) and
[Component and SDK checks](../development/architecture-validation.md).
