# Pipeline and material foundations

`PipelineDefinition` describes retained program source paths, the existing
position3/UV2 vertex layout, triangle/strip topology, fixed blend/depth/cull
intent, and a public parameter contract. `MaterialDefinition` references a
specific shared pipeline generation and supplies defaults, including retained
image handles and zero-based texture-unit requests. Neither definition contains
GLSL uniform names. Backend mappings are supplied to explicit OpenGL builders.

`Pipeline` is a backend base with a validated definition snapshot and no value
copying/slicing. Its backend subclass must retain the linked executable program;
compiled stages remain transient. `Material` copies its recipe on construction.
Definition/default access is read-only. Building another pipeline/material
instance preserves the old logical generation while its readers retain it.
This is the ownership foundation for reload; actual ShaderMgr/frame replacement
has not yet migrated to these types.

## Parameters and ownership

Parameter values own scalars, vectors, matrices, and their map keys. A sampler
owns a shared `const Image` handle plus a binding-unit request. Each pass and draw
must supply its own copied ParameterSet when the frame migration is implemented;
material defaults already belong to the immutable material snapshot. Resolution
returns a new owned set, never a view into the caller's inputs.

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
stores no unit. Current frame/legacy adapters also select zero explicitly.
Two material recipes can retain the same image with different unit requests
without mutating each other's recipe or the cached image. Sampling/filter/wrap
policy remains the image's existing upload policy; per-material sampler objects
are a separate extension, not claimed by this foundation.

## Remaining integration

Task 7 is unfinished. OpenGL pipeline building, explicit mappings/reflection,
required/optional validation, copied-value uploads, and sampler-domain/unit checks
are implemented in source. Frame and recipe integration remain pending.
GLSLPipeline::draw applies complete fixed state after validating pass constraints,
geometry, and parameters. Current legacy frames have not yet migrated to that path.
Linked attributes must match Vertex2D's
position3 at location zero and UV2 at location one; inactive inputs may be omitted.
Geometry exposes immutable CPU-readable layout/topology/count metadata. Pipeline
validation checks complete primitives and bounded ranges before publication;
native drawing additionally checks VAO/program domain identity and the live current
context. Program/image native domains are checked as well.

ShaderMgr still publishes retained Shader handles, and DrawStyle still stores one.
Successful pipeline/material recipe replacement and retained old-frame generations
must be wired into bootstrap/reload before 7.6 is complete. Task 8 must then resolve
asset/text commands into geometry/material packets outside the renderer.

Start with typed C++ definitions and explicit bootstrap/build APIs. A future
definition-file parser should produce these types separately from generic manifest
discovery. Do not add guessed shader-stage scanning to asset manifests.

The common regression sources exercise a two-image effect with time/color/intensity,
copied inputs, type/required/optional validation, immutable generations, failed
replacement construction, and independent unit selection. Native sprite and alpha
font traces, native state/reload tests, compilation, and execution remain
acceptance work. No real GL behavior is established by source preparation.

## Native bootstrap and binding

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

Six prepared recording-GLAD scenarios cover a native two-image effect, optional
uniform reset after another draw, inactive optional uniforms without sprite roles,
reflection/type/storage failures, invalid attributes/unlinked programs, and sampler
domain/unit failures before any bind. These sources use synthetic IDs and restore
all replaced entry points; they do not replace real-context acceptance.
