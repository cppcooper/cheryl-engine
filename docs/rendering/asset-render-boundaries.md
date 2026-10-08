# Asset and renderer header boundaries

`assets/types/` contains the logical 2D and 3D asset types and portable primitives.
`assets/types/2d/base/` holds shared bases for 2D asset types.
`assets/geometry/` builds temporary CPU geometry for asset loaders.
`assets/definitions/` describes grids, clips, sprites, tilesets, and autotile metadata.
`assets/definitions/manifest.h` collects those definitions into a parsed source document.
`assets/resources/` holds backend-neutral resource interfaces used by asset types
and resource providers. `core/rendering/` contains frame submissions, the renderer
contract, copied draw parameters, and resolved packets.

The [OpenGL module](../../projects/modules/graphics/opengl/README.md) owns concrete
resources, provider, renderer and context under its `backends/opengl/` headers.
Its headers are opt-in through
`<backends/opengl.h>`; `<assets.h>` and `<core.h>` do not include Glad.
Concrete resources may use OpenGL types, while shared asset and renderer
interfaces must not expose them.

`Geometry2D::bind()` selects geometry independently of `Image::bind(unit)`.
Units are zero-based requests owned by the draw/material; cached images retain
no mutable binding unit. CPU submission specifies image parameters and units.
The OpenGL renderer checks an image's backend independently of geometry binding.
Standard shader parameters use `ShaderPass` and `ShaderDraw`; the OpenGL material maps
semantic roles to its uniform names; material sampler requests use ImageBinding.
The legacy ShaderDraw path also carries a texture unit. Custom uniform APIs remain
available for application parameters. Common drawing code
does not select GLSL names.

Typed pipeline definitions, immutable material defaults, and copied custom
parameter resolution are described in [pipelines-and-materials.md](pipelines-and-materials.md).
Render packets retain a material generation and geometry, with copied parameter
values. Frame playback does not inspect a font, sprite, tileset, or animation.

CPU preparation owns RGBA pixels and temporary vertex data. Backend creation
accepts decoded images and transient vertex spans; it copies upload data before
returning. RGBA source rows are top-to-bottom; OpenGL uploads them bottom-up
to match the atlas UV convention. The caller's pixels remain intact. STB's one-channel
alpha atlas uses its own baked UVs and keeps its supplied row order. GPU handle destruction is coordinated by the OpenGL renderer while
its own context is current. Actual-context checks also apply to uploads and draw
operations. Runtime architecture and application APIs are described in
[runtime-architecture.md](../runtime/runtime-architecture.md).

## Legacy font resources

FFont keeps its typed alternate bank and normalized metrics; STBFont uses a baked
ASCII atlas. Their [migration and metric contract](../resources/legacy-ffont.md)
remains distinct from the [Unicode service](../assets/text-layout.md), whose retained
pages pair shaped placements with immutable resource generations.
