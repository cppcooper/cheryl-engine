# Cheryl Engine demo

The demo composes `Cheryl::Engine`, `Cheryl::NativeGLFW` and `Cheryl::OpenGL`.
WASD pans the camera, R resets it and F5 queues a shader reload while retaining the
previous material until replacement succeeds. `--concurrent` selects the separate
simulation owner; `--max-updates=N` makes a finite run.

When `Cheryl::UI::TGUI` is selected, the demo also links that optional module and
shows a TGUI panel. Set `CHERYL_BUILD_UI_TGUI=ON` and reload CMake in existing
profiles which cache it as `OFF`. Disabling the module retains the original
toolkit-free demo and its small F2 text-input probe.

The panel has live counters, a camera-reset button, an editable field, a scrolling
list, nested/overlapping translucent panels, a replaceable image and a tooltip near
the window edge. Click a widget or press F2 to acquire exclusive keyboard focus;
Escape or a click outside releases it. F2 toggles text focus, and F3 hides/shows the
panel. Text editing uses committed Unicode input with the toolkit's embedded font;
clipboard/IME services remain unavailable. Gamepad State continues independently
of keyboard focus. Pointer delivery is explicitly selected while the panel is
visible and does not suppress gameplay mouse State.

`Game` passes a neutral status model to `DemoUi` and receives a camera-reset action.
`DemoUi` builds its OpenGL materials on platform during initialization, then creates
the session/widgets lazily on the first simulation update. It feeds copied logical
and framebuffer dimensions and simulation time to the session. Complete CPU
recordings go through the existing platform dispatcher/uploader, with at most one
replacement pending. Upload failure preserves the current scene and appears in the
HUD. Frame preparation appends that retained UI scene after the world/HUD pass;
it never traverses live widgets. Teardown follows the runtime's simulation join and
releases application-held widgets before the global toolkit backend.

The adapter's controlled module checks are accepted; downstream composition and
native demo acceptance remain in
[U9](../../../docs/planning/cheryl-ui-integration-plan.md#remaining-development-sequence).
Once builds/tests are explicitly authorized, reuse an existing Debug build and
batch the affected demo/consumer targets with one low-priority build job. Reuse
accepted module results unless new changes require a rerun. Run the independent
consumer, then use a short normal and `--concurrent` demo run to check startup,
uploads and teardown. For visual acceptance, exercise text/focus, scrolling,
image replacement and the edge tooltip; resize the
window across available content scales. Finite runs alone do not prove interaction,
appearance or retained native resource behavior through those changes.
