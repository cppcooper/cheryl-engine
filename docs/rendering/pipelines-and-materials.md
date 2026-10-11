# Pipelines, materials, and render submission

`PipelineDefinition` describes retained program source paths, position3/UV2 or
position3/UV2/color4 vertex layout, triangle/strip topology, fixed blend/depth/cull
intent, and a public parameter contract. `MaterialDefinition` references a
specific shared pipeline generation and supplies defaults, including retained
image handles and zero-based texture-unit requests. Neither definition contains
GLSL uniform names. Backend mappings are supplied to explicit OpenGL builders.

`Pipeline` is a backend base with a validated definition snapshot and no value
copying/slicing. Its backend subclass must retain the linked executable program;
compiled stages remain transient. `Material` copies its recipe on construction.
Definition/default access is read-only. Building another pipeline/material
instance preserves the old logical generation while its readers retain it.
MaterialMgr publishes complete immutable recipes, including their pipeline
generation, through explicit application-supplied builders. Indexed definitions
use the separate `ShaderAssetMgr` catalogue and optional provider-owned
`ShaderAssetBuilder`. Both paths preserve immutable generations. RenderFrame packets
retain those generations for low-level renderer playback.

## Parameters and ownership

Parameter values own scalars, vectors, matrices, and their map keys. A sampler
owns a shared `const Image` handle, a binding-unit request and an optional retained
`const Sampler` override. Each pass and draw
supplies copied ParameterSet values; material defaults belong to the immutable
material snapshot. Resolution returns a new owned set, never a view into the
caller's inputs.

| Source | Ownership | Precedence |
| --- | --- | --- |
| Contract default | Pipeline generation | Lowest |
| Pass custom values | Pass submission | Overrides contract defaults |
| Material defaults/resources | Material generation | Overrides pass custom values |
| Draw custom values | Draw submission | Highest |

Projection/view/model/alpha/scale semantics come from ShaderPass/ShaderDraw.
The contract explicitly opts into each semantic; an effect does not inherit
sprite uniforms automatically. Custom layers cannot override semantic keys.
Custom values support float/int/uint/bool, float vec2/vec3/vec4, mat4, and sampler2D.
Matrix scope is deliberately limited to the existing mat4 transforms.

Contracts reject duplicate/empty keys, invalid types, duplicate or mistyped engine
semantics, and invalid defaults. Each source layer rejects unknown keys, wrong
types, semantic overrides, and null images, even if a later value would hide the
problem. After all layers resolve, required missing values and colliding sampler
units fail. Optional values with no source/default are absent from the result.
The native builder requires a reset/default for an active optional custom uniform.
An inactive optional uniform produces no upload. Missing active engine semantics
reject an incomplete packet. Binding rewrites every active contracted uniform, so
an absent optional value cannot inherit a previous draw's value.

## Independent binding

Geometry selects only its vertex array/buffer. Image binding accepts a zero-based
unit for each request; OpenGL converts it to GL_TEXTURE0 + unit and checks the
current context's unit limit, sampled once during upload rather than queried on
every draw. Image upload uses unit zero as temporary setup and
stores no unit. Asset submission selects the requested key/unit explicitly.
Two material recipes can retain the same image with different unit requests
without mutating each other's recipe or the cached image.

### Immutable sampling

`SamplerOptions` selects nearest/linear minification and magnification, no/nearest/
linear mipmap filtering, clamp/repeat/mirrored-repeat wrapping on each axis and
disabled/maximum-supported anisotropy. Engine defaults are linear filtering,
linear mipmaps, clamp-to-edge and maximum-supported anisotropy. OpenGL image
creation applies those defaults and generates a complete mip chain. Font-atlas
creation keeps its separate no-mipmap policy. Unsupported anisotropy resolves to
an effective value of one; the uploaded sampler exposes the effective value.

`ResourceProvider::create_sampler(options)` is an optional capability. Its default
rejects rather than silently dropping overrides. OpenGL creates immutable sampler
objects on the loading owner and weakly reuses live matching options. An
`ImageBinding` without an override uses the image's default policy. With an override,
the packet retains it independently of image pixels. An FX material's named sampling
policy fills a missing binding sampler; an explicit submitted sampler wins.
Ordinary indexed materials need no sampling declaration or image knowledge.

OpenGL validates sampler/image domains, unit bounds and mip completeness before
binding. A draw using default image sampling unbinds a preceding sampler object;
state from another draw cannot leak into it. Samplers retire through the native
resource lifetime just like textures. Shared images and previously published
frames remain unchanged when another binding selects different sampling.
For exact nearest sampling, disable anisotropy explicitly. Grid UVs still address cell edges;
sampling support does not supply atlas padding or prevent every adjacent-cell bleed.

## Frame and recipe integration

OpenGL pipeline builders validate explicit mappings/reflection, required/optional
values, copied-value uploads, and sampler domains/units. GLSLPipeline::draw applies
complete fixed state after validating pass constraints, geometry, and parameters.
RenderFrame packets use that path.
Linked attributes match position3 at location zero and UV2 at location one. The
colored layout additionally maps float RGBA at location two, with an explicit
`GLSLPipelineBindings::color_attribute` name. Inactive inputs may be omitted;
an active color input is rejected for the uncolored layout. `Vertex2D` keeps its
existing storage; `Vertex2DColor` is a separate immutable upload format. Pipeline
and geometry layouts must match. Shaders interpret color values consistently with
their selected straight/premultiplied blend mode; upload does not convert them.
Geometry exposes immutable CPU-readable layout/topology/count metadata. Pipeline
validation checks complete primitives and bounded ranges before publication;
native drawing additionally checks VAO/program domain identity and the live current
context. Program/image native domains are checked as well.

Common validation is CPU-only and reads stable inputs. Pipeline construction checks
source paths for emptiness, supported layout/topology/state and parameter contracts;
it does not read files, link or reflect a program. Material construction validates a
partial custom-default layer, so required values and sampler collisions are checked
when resolving a complete draw. `validate_parameter_values()` permits missing keys
and colliding units in a partial layer; `validate_resolved_parameters()` requires
required keys and distinct final sampler units, but does not prove that engine values
actually came from a camera. Neither checks image domains, backend unit limits,
numeric finiteness or alpha/scale ranges. Backend binding supplies its additional
validation. A validation/resolution failure leaves caller inputs and immutable recipes
unchanged.

### Rectangular clipping

`DrawStyle2D::clip` resolves into a copied optional `DrawPacket2D::clip`. A
`ClipRegion2D` retains top-left logical edges and positive logical viewport dimensions.
Edges must be finite and ordered; equal edges mean empty. `intersect_clip_rects`
combines nested rectangles in the same logical coordinate space. Clipping does not
transform with the draw's model matrix or change authored order.

Playback maps the retained extent to its current framebuffer. `resolve_clip_region`
clamps to the logical viewport and rounds nonempty edges outwards (floor left/top,
ceil right/bottom). Rectangles outside the viewport, zero-area rectangles and a
zero-sized framebuffer produce no pixels. This is integer rectangular clipping;
it does not promise subpixel masks or rotated clips. Resize/content-scale changes
use the playback framebuffer and the retained logical extent without reading live UI
state. A changed logical layout belongs to a newly published frame.

OpenGL converts top-left pixel edges to its bottom-left scissor box after validating
the complete draw. Empty clips issue no draw; absent clips disable scissor. Full
frame clears also disable scissor, so a preceding clipped packet cannot restrict
the next clear. The renderer viewport supplies the framebuffer extent.
Direct clipped `GLSLPipeline::draw` calls also supply that extent explicitly.

ShaderMgr publishes Shader handles for explicit program access; DrawStyle2D stores
an immutable Material handle.
MaterialMgr::load_material retains an existing key; reload_material builds a full
candidate outside cache locks, then replaces one recipe after success. Builders
own their typed definitions/backend mappings and validate all dependent resources.
A throw or null candidate retains the previous complete recipe; retained readers
keep the old material/pipeline/image generation. Provider teardown clears recipes
before other resource caches, with the same loading-owner/domain exclusion.
The demo loads indexed `main:text` and `main:images` recipes and reloads them on
the platform owner.
Frame preparation resolves asset/text submissions into owned packets; playback only
consumes geometry, material, ranges, and copied parameters. A failed demo reload keeps
the previous generation and reports its error in the overlay.

Graphics manifests 2.0 also produce owned CPU recipes through explicit index
selection. Shader/material documents contain no default images; sprites and
tilesets can select a material independently of their texture. The
[manifest guide](../assets/asset-manifests.md#shadermaterial-definitions) owns the
document format; the [loader guide](../assets/asset-loading.md#publication-and-retry)
owns catalogue ordering, preserve-existing loads and incremental replacement.

Use [architecture validation](../development/architecture-validation.md) for native
state/reload, retained-packet and fixture checks.

## Native bootstrap and binding

The OpenGL provider implements `ShaderAssetBuilder` without exposing GL types to
common loading. It validates opaque `bindings.opengl` payloads and links the owned
prepared source bytes. It constructs materials from the catalogue's retained program
recipe/executable pair. These optional APIs coexist with the manual builders below.

`OpenGLResourceProvider::build_pipeline(definition, bindings)` links the definition's
sources and constructs GLSLPipeline after validating reflection. GLSLPipelineBindings
maps public parameter keys to uniform names; its attribute-name mapping is also
backend-owned. `build_material(recipe)` checks that its pipeline and default images
belong to this provider's live native domain. Both builders require the platform
owner with its rendering context current. Common resource/definition headers remain
free of GL names and types.

The pipeline owns the linked GLSLProgram and immutable definition. Native stages
are marked for deletion after attachment and detached after linking, including a
failed link, so a retained executable does not retain its compilation stages.
Failure before linking is covered by the program/stage cleanup guards.

Every contract key needs an explicit unique uniform mapping. Required uniforms
must be active; optional ones may be absent after optimization. Active uniform
types must match exactly. The initial API rejects uniform arrays and block storage,
and requires every active uniform to appear in the contract. This prevents an
undeclared mutable program value from silently affecting another draw.

Material::resolve() produces copied values from engine semantics and custom layers.
Pass those values to GLSLPipeline::bind_parameters() on the platform owner. The
binder fills omitted active optional uniforms from contract defaults or the mapping's
missing_value, then validates the complete set, including fallback sampler collisions.
It checks all image domains/units before changing the program or texture bindings.
This method applies parameter/resource values only. GLSLPipeline::draw instead
validates the complete geometry/state/parameter request, reapplies blend equations
and factors, depth enable/function/write mask, cull enable/face, and CCW front-face
winding, then binds parameters and geometry and draws. Every draw sets all supported
state, so adjacent draws/passes cannot inherit another pipeline's policy. Depth
clears explicitly enable depth writes before clearing; the next draw reapplies its
own mask.

PassConstraints2D restricts any selected pipeline state without overriding it.
Default 2D passes require disabled depth; set or reset that constraint explicitly
for a depth pass or a mixed-policy pass. Opaque blending uses one/zero factors;
straight alpha uses source-alpha/one-minus-source-alpha RGB with one/one-minus-source-alpha
alpha; premultiplied alpha uses one/one-minus-source-alpha; additive uses source-alpha/one
RGB and one/one alpha. All use additive blend equations.

Recording-GLAD scenarios cover a native two-image effect, optional
uniform reset after another draw, inactive optional uniforms without sprite roles,
reflection/type/storage failures, invalid attributes/unlinked programs, and sampler
domain/unit failures before any bind. These sources use synthetic IDs and restore
all replaced entry points; they do not replace real-context acceptance.

## Manual material construction

TGUI and RmlUi deliberately keep manual recipes. The demo's
[TGUI builder](../../projects/apps/demo/src/tgui-demo.cpp) and
[RmlUi builder](../../projects/apps/demo/src/rmlui-demo.cpp) show adding typed
definitions and wiring their bindings explicitly. On the OpenGL loading owner,
with its context current, a textured TGUI material can be built as follows
(`native` is an `OpenGLResourceProvider&`, `root` is the asset root):

```cpp
using namespace CE::Assets;
PipelineDefinition definition;
definition.vertex_layout = VertexLayout2D::Position3UV2Color4;
definition.topology = PrimitiveTopology::Triangles;
definition.state = {BlendMode::StraightAlpha, DepthMode::Disabled, false, CullMode::None};
definition.program_sources = {root / "graphics/shaders/tgui.vert",
                              root / "graphics/shaders/tgui-textured.frag"};
definition.parameters = {{"projection", ParameterType::Mat4, true, ParameterSemantic::Projection},
                         {"image", ParameterType::Sampler2D}};
GLSLPipelineBindings bindings{{{"projection", "projectionMatrix"}, {"image", "mytexture"}}};
auto material = native.build_material({native.build_pipeline(std::move(definition), bindings), {}});
```

Supply that handle as `Materials::textured` to the UI uploader; its per-draw image
binding supplies the texture and sampling without putting an image in the recipe.
The solid material omits the image parameter and uses `tgui-solid.frag`. RmlUi uses
the same construction pattern with premultiplied-alpha blending. Required parameter
keys belong in the typed definition, and their uniform mappings belong in bindings;
adding one requires both and a matching shader uniform.

A future indexed UI migration targets TGUI first. If RmlUi also migrates, retain
this manual example and an independently usable manual consumer. Other shader
files are not indexed merely because they exist; their purposes and native behavior
remain unverified.

## CPU submission

DrawPacket2D retains geometry, an immutable material generation, and copied resolved
parameters. Asset submission helpers select sprite/tile ranges, whole graphics,
and laid-out font glyphs before rendering, without binding native resources.
ImageParameter2D selects a public sampler key and unit explicitly. Text layout is
const and returns glyph placements; deprecated FFont bank selection remains typed.
Resolved glyph packets retain the geometry/atlas even after the Font object is released.
RenderPassWriter validates individual packets or a whole glyph group before publication
and assigns stable authored order. RenderFrame keeps reusable vector capacity and
releases packet/pass handles on recycle. The OpenGL renderer checks its native
pipeline domain, then calls the validated fixed-state draw path in authored order.
Graphic/Tile/TileAnimation and text drawing use CPU submission helpers.
Their [asset/playback contract](../assets/asset-values-and-playback.md) defines
metadata and glyph units, direct-construction limits and image-key insertion.
Draw2D/iDraw/DrawInfo and the font formatting pointer contract are retired. Deprecated
FFont keeps immutable width metrics and typed normal/alternate-bank layout; callers
place and rotate text through DrawStyle2D.model_matrix. Legacy STBFont uses a supplied
font file and an ASCII atlas. The [Unicode service](../assets/text-layout.md#preparation-upload-and-retained-submission)
provides retained shaped glyph pages through its `RenderedText` submission overload.
See [FFont deprecation](../resources/legacy-ffont.md).

### Frame storage and writers

Frame writers borrow a slot; they do not lock it or publish it across threads.
The runtime's [frame boundary](../runtime/runtime-frame-boundary.md) owns that transfer.
Only active passes are visible through `passes()`, whose span borrows the slot and
is invalidated by outer-vector growth. A RenderPassWriter uses an index that survives
that growth, but references from `parameters()`/`constraints()` do not. Keep every
writer within one exclusive preparation phase, before publication or recycling.

Each `add()` validates before inserting. The group overload validates all packets
and reserves capacity before appending any of them. Validation/allocation failure
leaves that pass's earlier draws intact; it does not roll back an entire frame or
other application side effects. Writer destruction performs no commit or rollback.
Recycle a partially prepared frame before reuse. `begin_pass()` owns its values and
copies matrices; a pass's constraints are immutable during its preparation through
this writer API. Authored pass/draw order is the playback order regardless of the
packet's `order_sensitive` metadata.

Recycle only after playback or supersession excludes readers, on the resource
release owner required by the backend. It releases active packet/pass handles and
keeps vector capacity for later preparation. Native render/presentation failures
can leave partial output or changed native state; CPU validation is not a transaction
over backend execution.

### Camera observations

CameraBase is unsynchronized mutable CPU state. Setters and matrix references belong
to one owner; a revision is a change counter, not an atomic publication mechanism.
Copy matrices into a pass before sharing them with playback. Changed framebuffer
size, view or perspective advances the revision once, while equal configurations
leave it unchanged. View equality is exact per component; view matrices are not
checked for finiteness or invertibility.

Framebuffer dimensions are pixels. Negative dimensions throw before changing the
camera; zero dimensions are retained while projection clamps each axis to at least
one. Camera2D uses GLM orthographic bounds `[0, width]` and `[0, height]`, Y-up, with
near/far 0/1. Camera3D uses vertical FOV in degrees and framebuffer aspect; finite
`0 < FOV < 180` and `0 < near < far` are required, with plane distances in view units.
Invalid perspective values leave the old configuration unchanged. Neither camera
selects renderer depth policy or converts logical window coordinates to framebuffer
pixels; that choice belongs to the consumer.
