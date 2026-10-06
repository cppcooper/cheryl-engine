# RmlUi UI integration

`Cheryl::UI::RmlUi` is the optional, independently selected RmlUi 6.3 owner.
It links `Cheryl::Engine` and `RmlUi::Core` with the stock FreeType font engine.
The module provides portable key/modifier translation, CPU recording and retained
scene upload. Session, native integration and acceptance are active in
[U9](../../../../docs/planning/cheryl-ui-integration-plan.md#remaining-development-sequence).

The [requirements](../../../../docs/planning/rmlui-adapter-requirements.md) define
the selected rendering/font/lifetime scope. The
[adapter-author guide](../../../../docs/development/ui-adapters.md) describes the
engine boundaries; RmlUi keeps its native context, RML and RCSS authoring API.

## Selection and dependency

Set `CHERYL_BUILD_UI_RMLUI=ON` and reload CMake to expose `cheryl_ui_rmlui`,
`ui-rmlui-tests` and `ui-rmlui-all` when tests are selected. The root option defaults
to `OFF`; TGUI and RmlUi can be selected separately or together.

```cmake
target_link_libraries(game PRIVATE Cheryl::UI::RmlUi)
```

The module reuses an existing `RmlUi::Core`. Otherwise it uses an explicit
`CHERYL_RMLUI_SOURCE`, an explicit package location in `RmlUi_DIR`, the pinned
`extern/rmlui` submodule, or the exact package, in that order. Initialize the bundled
dependency with `git submodule update --init extern/rmlui`; CMake does not download
it. Supplied targets must declare `RMLUI_FONT_ENGINE=freetype`; ordinary source and
package configurations provide that metadata.

An owned source configuration selects FreeType/Core and excludes samples, toolkit
tests, native backends, Lua and optional plugins. Conflicting settings reject
instead of rewriting a host's choices. The module links Core directly, leaving
the upstream debugger outside the adapter dependency graph.

For a standalone module or its `tests/consumer/` entry point, supply
`Cheryl::Engine` or `CHERYL_ENGINE_SOURCE`. Engine bootstrap suppresses other
module/demo/test selection in its local scope. Consumer/header checks are selected
through `CHERYL_BUILD_CONSUMER_TESTS` at the root or by configuring the consumer
entry point independently.

## Input identities

`keyboard_key` maps portable physical keyboard identities to RmlUi keys; unknown
keys remain `KI_UNKNOWN`. Committed characters come from `TextEvent`, not from
this mapping. `modifiers` preserves Control, Shift, Alt, Super, Caps Lock and Num
Lock flags in RmlUi's modifier representation.

## Focused checks

`ui_rmlui_input.*` covers representative keys and combined modifiers.
`ui_rmlui_recording.*`, `ui_rmlui_texture.*` and `ui_rmlui_scene.*` cover owned
geometry/pixels, alpha/orientation, clipping, retained frames, provider ownership,
material validation and failure-preserving adoption. A checked-in two-row image
distinguishes file premultiplication and row order from generated texture handling.
The independent consumer links only this adapter; its header probes use public
headers as the first include. Executable acceptance remains in U9.

## Recording and publication

`RenderTarget` implements the Core render interface without graphics calls. It
copies indexed triangles during compilation and translated draws during recording.
Texture handles retain immutable decoded generations; releasing a toolkit handle
does not invalidate a completed recording. Generated premultiplied pixels remain
unchanged; decoded file RGB is multiplied by alpha once. Texture V coordinates
map RmlUi's downward-positive convention to Cheryl's upward-positive image UVs.

Rectangular scissor edges and logical dimensions travel with every draw. Zero
dimensions or empty clips produce no draws. Masks, nonidentity transforms, layers,
filters, custom shaders and repeating texture coordinates reject publication.
The failure stays latched for that renderer because the toolkit may cache an
unsupported compiled property without offering it again. Recreate the session
after removing such a property. SDK callbacks allow render-stack cleanup to finish
before the recording reports the error.

The application supplies `Materials` with colored triangle pipelines,
`PremultipliedAlpha`, disabled depth/write/culling, a projection semantic and a
custom image sampler for textured draws. Shaders multiply image RGBA by vertex
RGBA without an additional premultiplication. `SceneUploader` belongs to one
provider/domain and platform owner; `submit` transfers owned recording data through
Cheryl's dispatcher. `adopt_scene` never waits and replaces the retained scene only
after a complete successful upload. Failure or cancellation preserves its predecessor.
