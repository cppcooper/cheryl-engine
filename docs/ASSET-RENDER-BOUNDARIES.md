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

`Geometry2D::bind(Image)` pairs neutral geometry and image handles. Standard
shader parameters use `ShaderPass` and `ShaderDraw`; the OpenGL material maps
semantic roles to its uniform names, including the selected texture unit. Custom
uniform APIs remain available for application parameters. Common drawing code
does not select GLSL names.

CPU preparation owns RGBA pixels and temporary vertex data. Backend creation
accepts decoded images and transient vertex spans; it copies upload data before
returning. GPU handle destruction is coordinated by the OpenGL renderer while
its own context is current. Actual-context checks also apply to uploads and draw
operations. Architecture and API migration are described in
[API-ABSTRACTION-PLAN.md](API-ABSTRACTION-PLAN.md).
