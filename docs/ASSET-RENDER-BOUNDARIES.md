# Asset and renderer header boundaries

`assets/types/` contains the logical 2D and 3D asset types and portable primitives.
`assets/types/2d/base/` holds shared bases for 2D asset types.
`assets/geometry/` builds temporary CPU geometry for asset loaders.
`assets/definitions/` describes grids, clips, sprites, tilesets, and autotile metadata.
`assets/definitions/manifest.h` collects those definitions into a parsed source document.
`assets/resources/` holds backend-neutral resource interfaces used by asset types
and resource providers. `core/rendering/` contains frame submissions, the renderer
contract, and legacy immediate-draw parameters.

`backends/opengl/` contains the concrete OpenGL resources, provider, renderer,
context, and GLFW composition. Its headers are opt-in through
`<backends/opengl.h>`; `<assets.h>` and `<core.h>` do not include Glad.
Concrete resources may use OpenGL types, while shared asset and renderer
interfaces must not expose them.

`Geometry2D::bind()` selects geometry independently of `Image::bind(unit)`.
Units are zero-based requests owned by the draw/material; cached images retain
no mutable binding unit. Existing immediate/frame adapters select unit zero.
The OpenGL renderer checks an image's backend independently of geometry binding.
Standard
shader parameters use `ShaderPass` and `ShaderDraw`; the OpenGL material maps
semantic roles to its uniform names, including the selected texture unit. Custom
uniform APIs remain available for application parameters. Common drawing code
does not select GLSL names.

Typed pipeline definitions, immutable material defaults, and copied custom
parameter resolution are described in [PIPELINES-AND-MATERIALS.md](PIPELINES-AND-MATERIALS.md).
These foundations do not yet replace the legacy frame's Shader handle.

CPU preparation owns RGBA pixels and temporary vertex data. Backend creation
accepts decoded images and transient vertex spans; it copies upload data before
returning. GPU handle destruction is coordinated by the OpenGL renderer while
its own context is current. Actual-context checks also apply to uploads and draw
operations. Architecture and API migration are described in
[API-ABSTRACTION-PLAN.md](API-ABSTRACTION-PLAN.md).
