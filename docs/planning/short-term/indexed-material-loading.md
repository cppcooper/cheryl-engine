# Shader/material manifests and indexed loading

## Goal and scope

Define a versioned shader manifest schema and make shader/material loading follow
the same explicit discovery and loading flow as sprites and tilesets: discover files,
select graphics definition documents
through `graphics-manifests.json`, prepare owned CPU recipes, then construct and
publish resources on the active backend's loading owner.

This is a [near-term design unit](../develop-review-and-development-plan.md#nearest-planned-work).
Shader/material document identity and backend construction remain open design
decisions; scheduling does not approve them or authorize implementation/execution.
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

## Design checkpoint

Resolve these contracts in discussion before extending schemas or public APIs:

1. **Document format and dispatch.** Decide between extending the existing asset
   definition format and introducing shader/material-specific formats. Specify
   whether independently identified shader/program documents are referenced by
   materials or one definition format owns both recipes. Define the shader manifest
   schema, version compatibility and how already-index-selected documents choose
   their parser.
   Keep graphics indexes as selection documents; do not add asset-type tags to
   them merely to duplicate a definition document's identity. Clarify the role of
   `$schema`: the existing leaf parser treats it as a format marker, not a schema
   file to fetch at runtime.
2. **Recipe contents and identity.** Define shader/program and material identities,
   cache keys and reference ownership where separate documents are selected,
   explicit shader stages, topology/layout/state, parameter contracts and supported
   defaults. Define duplicate-ID behavior and how two materials share sources
   without colliding. Decide how backend-specific binding names are represented;
   keep their interpretation backend-owned. Defer features beyond the two demo
   recipes unless required for a coherent format.
3. **Backend construction seam.** Choose an optional provider capability or an
   explicitly supplied backend recipe-builder adapter. Common loading must not
   depend on OpenGL or require every existing provider to implement graphics
   material creation. Unsupported recipes/backends must fail explicitly.
4. **Prepared ownership and dependencies.** Choose whether preparation snapshots
   shader bytes or retains resolved source paths. Current OpenGL builders reopen
   shader paths during construction; byte snapshots require a corresponding
   builder/source API. Define default-image dependencies if supported without
   putting runtime image handles in prepared data.
5. **Demo selection and reload.** Materials are required in both normal and
   `--full-assets` modes. Choose a focused loading route that uses the same
   definition format and backend construction without forcing the default demo
   to decode every optional image. Define how F5 explicitly rereads/rebuilds the
   selected material definitions while preserving failure-safe replacement.

Approval of these decisions should establish a bounded implementation phase,
with a checkpoint if the schema, provider interface or demo-mode requirements
change materially.

## Implementation order after approval

1. Define the approved versioned shader/material schema, identifiers and CPU-only
   prepared recipe types.
   Add definitions for the two demo materials under `assets/graphics/shaders/`
   and select them through a local graphics index.
2. Extend indexed document dispatch and recipe parsing. Resolve registered shader
   paths, validate common recipe structure, reject collisions and preserve useful
   document/property locations in failures before native construction begins.
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
