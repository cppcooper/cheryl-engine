# Asset and renderer header boundaries

`assets/types/` contains the logical 2D and 3D asset types and portable primitives.
`assets/types/2d/base/` holds shared bases for 2D asset types.
`assets/geometry/` builds temporary CPU geometry for asset loaders.
`assets/definitions/` describes grids, clips, sprites, tilesets, and autotile metadata.
`assets/definitions/manifest.h` collects those definitions into a parsed source document.
`assets/resources/` holds backend-neutral resource interfaces used by asset types
and resource providers. `core/rendering/` contains frame submissions, the renderer
contract, copied draw parameters, and resolved packets.

`backends/opengl/` contains the concrete OpenGL resources, provider, renderer,
context, and GLFW composition. Its headers are opt-in through
`<backends/opengl.h>`; `<assets.h>` and `<core.h>` do not include Glad.
Concrete resources may use OpenGL types, while shared asset and renderer
interfaces must not expose them.

`Geometry2D::bind()` selects geometry independently of `Image::bind(unit)`.
Units are zero-based requests owned by the draw/material; cached images retain
no mutable binding unit. CPU submission specifies image parameters and units.
The OpenGL renderer checks an image's backend independently of geometry binding.
Standard
shader parameters use `ShaderPass` and `ShaderDraw`; the OpenGL material maps
semantic roles to its uniform names, including the selected texture unit. Custom
uniform APIs remain available for application parameters. Common drawing code
does not select GLSL names.

Typed pipeline definitions, immutable material defaults, and copied custom
parameter resolution are described in [PIPELINES-AND-MATERIALS.md](PIPELINES-AND-MATERIALS.md).
Render packets retain a material generation and geometry, with copied parameter
values. Frame playback does not inspect a font, sprite, tileset, or animation.

CPU preparation owns RGBA pixels and temporary vertex data. Backend creation
accepts decoded images and transient vertex spans; it copies upload data before
returning. RGBA source rows are top-to-bottom; OpenGL uploads them bottom-up
to match the atlas UV convention. The caller's pixels remain intact. STB's one-channel
alpha atlas uses its own baked UVs and keeps its supplied row order. GPU handle destruction is coordinated by the OpenGL renderer while
its own context is current. Actual-context checks also apply to uploads and draw
operations. Architecture and API migration are described in
[RUNTIME-ARCHITECTURE.md](RUNTIME-ARCHITECTURE.md).

## Legacy FFont compatibility decision

FFont's CPU layout and text submission replace its stored message, fancy flag,
print angle and unchecked format cast. The old implementation at checkpoint 43
and the original task base was compared with the migrated code during the audit.
The meaningful atlas contract is retained; incidental unsafe draw behavior is
explicitly changed as follows.

| Behavior | Migrated contract |
| --- | --- |
| Glyph selection | Printable ASCII selects glyph `letter - 32`; typed alternate-bank selection adds 128. Both banks retain their own immutable widths. |
| Pen advance | Glyph/space width is divided by 128, then multiplied by DrawStyle2D.scale during submission. Space advances without a packet. |
| Newline | Reset local x and subtract 1/128 from local y; submission applies the caller's scale and model to every line. |
| Rotation | One caller model controls all glyphs/lines. The old first-line rotation by scale and subsequent-line rotation by print_angle are not preserved. Newlines follow the model's local axes instead of resetting a world-space origin. |
| Unsupported bytes | Controls/high bytes use `?`, except newline and ignored carriage return. A tab also uses fallback in FFont; STBFont has its separately documented four-space advance. Old unchecked/signed byte indexing is removed. |
| State and lifetime | Layout does not store caller text/format. Each resolved glyph packet retains geometry, material and any supplied atlas binding. |

The recording submission sources cover typed banks, different bank widths,
space/newline advances, carriage return, control/high-byte fallback and a rotated,
scaled multiline model without native calls. These sources are uncompiled and
unexecuted. File-read/format-validation TODOs in the old loader remain outside this
migration; no new FFont loader or Unicode shaping is supplied.
