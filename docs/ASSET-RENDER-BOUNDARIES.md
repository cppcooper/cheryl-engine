# Asset and renderer header boundaries

`assets/types/` contains the logical 2D and 3D asset types and portable primitives.
`assets/manifest.h` describes source assets and their grids, clips, and metadata.
`assets/resources/` holds backend-neutral resource interfaces used by asset types
and resource providers. `core/rendering/` contains frame submissions, the renderer
contract, and legacy immediate-draw parameters.

`backends/opengl/` contains the concrete OpenGL resources, provider, renderer,
context, and GLFW composition. Its headers are opt-in through
`<backends/opengl.h>`; `<assets.h>` and `<core.h>` do not include Glad.
Concrete resources may use OpenGL types, while shared asset and renderer
interfaces must not expose them.

This layout does not yet change the resource contracts. `Geometry2D::bind(Image)`,
shader uniform binding, texture-unit selection, and context-bound GPU destruction
still need their own design and implementation passes.
