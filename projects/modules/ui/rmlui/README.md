# RmlUi UI integration

`Cheryl::UI::RmlUi` is the optional, independently selected RmlUi 6.3 owner.
It links `Cheryl::Engine` and `RmlUi::Core` with the stock FreeType font engine.
The initial module provides portable key/modifier translation and independent
consumer/header composition; recording, session, native integration and acceptance
are active in [U9](../../../../docs/planning/cheryl-ui-integration-plan.md#remaining-development-sequence).

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

`ui_rmlui_input.*` covers representative keys and combined modifiers. The independent
consumer links only this adapter and verifies the pinned Core version; its header
probes use public headers as the first include. These checks are authored and need
executable acceptance alongside the remaining adapter work.
