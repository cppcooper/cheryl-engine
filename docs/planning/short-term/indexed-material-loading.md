# Shader/material manifests and indexed loading

## Goal and scope

Define a versioned shader manifest schema and make shader/material loading follow
the same explicit discovery and loading flow as sprites and tilesets: discover files,
select graphics definition documents
through `graphics-manifests.json`, prepare owned CPU recipes, then construct and
publish resources on the active backend's loading owner.

This is a [near-term design unit](../develop-review-and-development-plan.md#nearest-planned-work).
The document and construction contracts below are approved for source implementation.
Keep manual material-builder APIs available to applications that do not opt into
automated loading. Audio and font loading are outside this extension.

## Current foundation

File discovery and indexed graphics loading were introduced in `5953fe2` and
`acd0170`. The implementation, rather than older asset guides, is the baseline:

- [Loader](../../../projects/engine/include/cheryl/core/resources/asset-management/asset-loader.h)
  uses a fresh `FileMgr` scan. Preparation owns a private file registry and changes
  no runtime caches; upload publishes registered paths and graphics resources.
- The registry retains JSON, WAV and shader paths. Only the exact basename
  `graphics-manifests.json` selects indexes; unlisted JSON is not opened.
- Index references resolve relative to their index through exact registered
  paths. Repeated references are parsed once; indexes cannot reference indexes.
  Selected documents currently use the sprite/tileset asset-manifest 1.0 format.
  The existing graphics-index and asset schemas do not define shader/program recipes.
- The demo still constructs two material recipes in
  [demo-game.cpp](../../../projects/apps/demo/src/demo-game.cpp), synthesizes shader
  filenames and downcasts the provider to OpenGL. The recipes share shader sources
  but use different topologies: triangles for text and triangle strips for images.
  F5 reconstructs these recipes for explicit replacement.
- [ResourceProvider](../../../projects/engine/include/cheryl/assets/resources/resource-provider.h)
  has no backend-neutral pipeline/material construction interface. Construction
  currently lives on `OpenGLResourceProvider`.
- `PipelineDefinition` is a CPU recipe, but `MaterialDefinition` retains a built
  pipeline and defaults that can own image handles. It is not a suitable prepared
  document representation as-is.

## Constraints to preserve

Use the existing graphics-index flow, not a scan-and-parse of every JSON file.
Place material documents and their index beside the shaders they describe.
Resolve shader references relative to their definition document using exact
registered paths; do not select a global first basename match or infer material
recipes by pairing shader filenames.

Prepared material data must contain no provider, executable shader, pipeline,
material or uploaded image handles. OpenGL construction and GLSL binding details
belong in the OpenGL module, not common asset-loading code or the demo game.

Preserve one active provider/cache domain and serialized loading-owner uploads.
Build and validate a replacement candidate before publishing it. Existing frame
owners retain their immutable material/pipeline generations; a failed explicit
reload retains the previous material. Ordinary loading preserves existing keys.
The loader remains incrementally publishing, not an atomic batch or implicit hot
reload system.

## Approved contracts and migration scope

Shader and material name the same asset class. A single versioned definition
document owns named executable-program recipes and named material recipes; this
internal distinction permits sharing a program between different draw topologies
without creating separate shader and material asset classes. Documents declare
`asset_class: "shader"` and `version: "1.0"`; `$schema` is an editor hint, never
fetched at runtime. Graphics indexes continue to select document paths only.

Definitions contain no image paths or default images. Texture definitions do not
refer to shaders. Sprite and tileset declarations own both their texture and an
optional qualified shader/material selection. Add this selection in asset-manifest
1.1, preserving the existing 1.0 contract. Uploaded assets retain the selected
material generation; an explicit draw-style material can override it. References
must resolve within the selected preparation batch before native construction.

Sampling remains engine-owned and anisotropic by default where supported.
Ordinary material recipes omit sampling policy. An FX recipe may explicitly
override sampling for a named sampler parameter without identifying an image;
images continue to arrive through asset submission or draw parameters.

Use qualified `namespace:name` identities, exact document-relative source paths,
owned shader-byte snapshots and CPU-only parameter literals. Reject duplicate
identities, references of the wrong kind and unsupported versions. Keep backend
bindings opaque to common loading. An optional provider-owned construction
capability validates and builds the recipes; existing providers need not implement
it, and manual material builders remain available.

A named catalogue shares the existing provider/cache domain and retains each
program's recipe together with its executable generation. Material construction
uses the retained program generation, including on preserve-existing loads.
Explicit replacement constructs candidates before publication. Batch publication
remains incremental; the demo adopts its text/image pair only after both reloads
succeed. Existing frames and assets retain their previous generations.

Migrate only the main demo's text and image materials. Other checked-in shaders
have unverified purposes or behavior and must not be indexed merely because they
exist. Default startup selects the main shader index without decoding all optional
images; full-assets startup and F5 use the same recipe interpretation.

TGUI and RmlUi retain their current manual material construction in this unit.
Future UI migration should target TGUI first. If RmlUi later migrates, preserve or
add a separate manual-construction example showing how to add definitions and wire
their parameters and backend bindings explicitly. Replacing both manual examples
without that demonstration would lose an intentional integration example.

The approved progression is test design, production implementation and test
implementation, completing each mode before advancing. Build/test execution and
general documentation synchronization remain separate authorization boundaries.

Version 1 covers vertex/fragment programs, both existing 2D vertex layouts,
triangles and triangle strips, current blend/depth/cull state, scalar/vector/matrix
parameters and sampler contracts. Arrays, uniform blocks, additional shader stages
and advanced FX are deferred. OpenGL interprets its own attribute/uniform binding
payload and validates active reflection before publication.

Stop for a decision if implementation materially invalidates these contracts or
requires expanding the migration beyond the two main demo recipes.

## Implementation order

1. Define the approved versioned shader/material schema, identifiers and CPU-only
   prepared recipe types.
   Add definitions for the two demo materials under `assets/graphics/shaders/`
   and select them through a local graphics index.
2. Extend indexed document dispatch and recipe parsing. Resolve registered shader
   paths, validate common recipe structure, reject collisions and preserve useful
   document/property locations in failures before native construction begins.
   Extend sprite/tileset manifests and retained submission values with optional
   shader selection, preserving independent texture and shader definitions.
3. Implement the approved backend construction seam in the OpenGL module. Retain
   manual `MaterialMgr` builders and their candidate-before-publication behavior.
4. Extend Loader preparation/upload, including dependency ordering, retained
   metadata and publication diagnostics. Preserve prepare-side isolation and
   explicitly report partial upload failures rather than promising rollback.
5. Replace demo path synthesis and inline recipes with definition selection and
   loaded-material retrieval. Route F5 through the same recipe interpretation.
   Keep `main.cpp` illustrative; isolate material-example orchestration in a
   focused header/translation unit only if it makes the example clearer.

Before implementation, inspect the shared working tree again and preserve unrelated
changes. Do not treat unfinished user work as a stable foundation for the extension.

## Validation and acceptance phases

Follow the [work-mode progression](../develop-review-and-development-plan.md#work-mode-progression):
test design, test implementation, execution and verification are separate phases.
Build/test execution and testing-request maintenance require their own authorization.
Carry the following requirements into those phases:

- Parser/registry coverage: valid and invalid shader/material recipes and versions,
  selected document dispatch, duplicate identities and program references if selected,
  exact-path resolution with duplicate basenames, missing shader references,
  repeated index references and malformed unlisted JSON remaining unopened.
- Preparation/upload coverage: no runtime mutations on preparation failure,
  owned staging lifetime, explicit unsupported-backend rejection, dependency
  ordering, loading-owner enforcement, partial publication and retry behavior.
- Material coverage: both topologies sharing sources, contract/binding validation,
  successful replacement, failed replacement preserving the cached generation,
  and previously retained frames remaining valid.
- Demo QA: default and full-assets startup, unchanged text/image rendering,
  successful F5 reload, invalid/missing source or definition handling and teardown.
- Existing manifest and runtime-adapter fixtures still assume root-level JSON
  auto-loading. Migrate their discovery/fixtures to relocated definitions and
  explicit graphics indexes before using them as acceptance for this extension.

Completion requires materials to use the indexed prepare/upload pattern without
inline demo shader recipes, while retaining manual loading and failure-safe
reload. Source implementation alone is not executable acceptance.

Once contracts stabilize, recommend a documentation-maintenance checkpoint for
the asset loading/manifest/discovery guides, asset catalogue, pipeline/material
guide, demo guide and affected testing requests. That synchronization is deferred,
not part of adding this plan.
