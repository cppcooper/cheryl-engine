# Pipeline and material foundations

`PipelineDefinition` describes retained program source paths, the existing
position3/UV2 vertex layout, triangle/strip topology, fixed blend/depth/cull
intent, and a public parameter contract. `MaterialDefinition` references a
specific shared pipeline generation and supplies defaults, including retained
image handles and zero-based texture-unit requests. Neither definition contains
GLSL uniform names. Backend mappings will be supplied to explicit builders.

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
The native builder must decide whether an optional uniform is inactive or require
a reset/default for an active optional value; skipping a write and inheriting a
previous draw's uniform is not an acceptable implementation.

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

Task 7 is unfinished. OpenGL pipeline building still needs backend parameter
mapping/reflection, required/optional native validation, and copied-value uploads.
Pipeline fixed state is currently validated intent; the renderer does not yet
apply it or enforce pass constraints. Geometry layout/topology, program/image
domain compatibility, and state reset across adjacent draws/passes remain open.
Native builders must validate before returning a publishable pipeline.

ShaderMgr still publishes retained Shader handles, and DrawStyle still stores one.
Successful pipeline/material recipe replacement and retained old-frame generations
must be wired into bootstrap/reload before 7.6 is complete. Task 8 must then resolve
asset/text commands into geometry/material packets outside the renderer.

Start with typed C++ definitions and explicit bootstrap/build APIs. A future
definition-file parser should produce these types separately from generic manifest
discovery. Do not add guessed shader-stage scanning to asset manifests.

The current regression sources exercise a two-image effect with time/color/intensity,
copied inputs, type/required/optional validation, immutable generations, failed
replacement construction, and independent unit selection. Native sprite and alpha
font traces, native reflection/state/reload tests, compilation, and execution remain
acceptance work. No real GL behavior is established by source preparation.
