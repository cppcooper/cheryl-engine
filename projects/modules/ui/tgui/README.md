# TGUI UI integration

`Cheryl::UI::TGUI` is an optional UI owner. Its current implementation translates
selected Cheryl input records into TGUI events. The retained render/resource bridge
and toolkit-session/GUI lifecycle remain in the
[U9 checklist](../../../../docs/planning/cheryl-ui-integration-plan.md#remaining-development-sequence).
This target does not yet provide a usable widget-rendering backend.

The module links `Cheryl::Engine` and TGUI 1.13.0, configured with a custom backend
and FreeType font support only. It has no Native GLFW/OpenGL/Gainput link. Public
headers expose TGUI's native event API; Engine headers never include TGUI. The
[requirements](../../../../docs/planning/tgui-adapter-requirements.md) define the
materials, font ownership, upload/lifetime and platform decisions for the adapter.

## Selection and dependency

At the repository root, select `CHERYL_BUILD_UI_TGUI=ON`. It defaults to `OFF` and
is independent of native/graphics selection. Link the application to
`Cheryl::UI::TGUI`; its Engine and toolkit requirements propagate through the target.

The module reuses an existing `TGUI::TGUI` target. Otherwise it uses an explicit
`CHERYL_TGUI_SOURCE` pointing to a TGUI 1.13.0 checkout, or finds that exact version's
CMake package. No dependency is downloaded. Source composition selects only the
custom/FreeType backend and excludes upstream tools, examples and tests. Conflicting
backend settings fail explicitly; supplied targets/settings are not rewritten.
Compiled version/feature checks also reject a supplied package/target with an
unreviewed version or native toolkit backend.

For a standalone module, supply `Cheryl::Engine` or `CHERYL_ENGINE_SOURCE`. Engine
bootstrap suppresses UI/native/graphics/demo/test selection in its local scope.
For an independently configured consumer, use `tests/consumer/` as the source
directory with the same dependency arguments; it links only this module and checks
its public header as the first include.

## Input translation

`translate_event(record)` reads portable keyboard/mouse identities and committed
text, preserving the original record for other readers. It does not choose focus,
capture or pointer routing. The application selects the eligible records and
dispatches translated events in their original order on the simulation owner.

Press and Repeat become TGUI `KeyPressed`; keyboard releases produce no toolkit
event because TGUI's event API has none. A GUI integration still updates its
modifier snapshot from the complete original stream, including releases.
Unsupported keys/devices/buttons return no event. Gamepad State remains available
to gameplay and does not become invented keyboard input.

Pointer coordinates are logical window coordinates floored to integer values;
negative fractional coordinates remain outside the window. Clicks and vertical
wheel events use their own observation-time positions and reject missing, nonfinite
or unrepresentable coordinates. Configure the GUI's input view in those units.
Fractional vertical wheel offsets are preserved; TGUI has no horizontal-wheel
event, so the original Cheryl record remains the source for another consumer.
Invalid Unicode scalars or nonfinite/unrepresentable wheel offsets reject explicitly.

## Focused checks

The module owns `ui-tgui-tests`, the `ui-tgui-all` aggregate and
`cheryl-ui-tgui-consumer`. Its cases join `all-tests` only when this module is
selected. These input checks require no window, graphics context, font file or
installed toolkit-global backend; widget/native acceptance belongs to later U9 work.

After explicit build/test authorization, a standalone source composition is:

```sh
cmake -S projects/modules/ui/tgui -B build-ui-tgui \
  -DCHERYL_ENGINE_SOURCE=/path/to/cheryl-engine \
  -DCHERYL_TGUI_SOURCE=/path/to/TGUI-1.13.0 \
  -DCHERYL_BUILD_TESTS=ON -DCHERYL_BUILD_CONSUMER_TESTS=ON
nice -n 19 cmake --build build-ui-tgui --parallel 1 \
  --target ui-tgui-tests cheryl-ui-tgui-consumer
./build-ui-tgui/ui-tgui-tests
./build-ui-tgui/cheryl-ui-tgui-consumer
```

Batch the changed Engine/native input cases with this validation when their
executable acceptance is needed. Reuse existing directories and avoid repeating
the accepted neutral rendering probe or aggregate suites.
