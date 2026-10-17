# GUI construction

Cheryl integrates existing UI toolkits with its ordered input and retained
rendering contracts. The application owns the session and interface objects;
the toolkit supplies widgets or documents. A text score drawn directly through
the rendering system is useful HUD content, but interactive UI also needs
widget construction, input routing, focus and a publication path.

## Choose an interface

| Integration | Application authors | Guide |
| --- | --- | --- |
| TGUI | Native C++ widgets and layouts through the session's `gui()` object. | [TGUI owner guide](../modules/tgui.md) |
| RmlUi | Native contexts, RML documents and RCSS styles through a document session. | [Create an editable document](../modules/rmlui.md#create-an-editable-document) |

Neither integration provides a common widget API. Select one according to the
application's authoring needs. Both convert ordered Engine input and publish
retained scenes; Engine headers do not import either toolkit.

## Construct widgets on the UI owner

Create the session before toolkit objects, on the thread that will own its UI
updates. In concurrent runtime mode that is the simulation thread, so create it
on the first update rather than during platform-owned game initialization.
Construct platform-owned materials/upload helpers during `init()` instead.

For TGUI, a session is constructed with the input system and a distinct nonzero
focus target. A native button and edit field are ordinary toolkit objects:

```cpp
auto button = tgui::Button::create("Apply");
button->setSize({120, 32});
button->setPosition({16, 16});
button->onPress([this] { apply_requested_ = true; });
session_->gui().add(button);

auto field = tgui::EditBox::create();
field->setSize({240, 32});
field->setPosition({16, 60});
field->setDefaultText("Type a name");
session_->gui().add(field);
```

This fragment belongs to an application class retaining `session_` and the
callback's target state. Include the toolkit's Button/EditBox headers and link
the selected UI target. It creates an interactive interface; upload/publication
is still required before it appears on screen. The maintained
[TGUI demo](../../projects/apps/demo/src/tgui-demo.cpp) supplies the complete
material, widget and scene wiring.

## Route input and focus explicitly

Update the session's view from `tick.logical_size` and
`tick.framebuffer_size`; do not read the live window on simulation. Feed the
complete `tick.input.records()` batch once, with an explicit decision about
which UI receives pointer input. Do not prefilter away modifier releases.

When opening an editor, call `request_keyboard_focus()` and, when closing it,
`release_keyboard_focus()`. Exclusive focus withholds keyboard gameplay actions
while text is directed to the UI; pointer selection is a separate application
choice. Give different sessions different focus targets. Requests affect later
polls, so pending records retain the focus epoch under which they were captured.
Session `before_record` callbacks support ordered application controls without
rewriting that entry epoch.

Ending exclusive focus resumes currently held keyboard controls on the next poll.
If Escape both releases an editor and quits gameplay, suppress that release gesture
until its physical key release has been processed. The [GUI task](../getting-started/how-to/create-gui.md#feed-records-and-control-focus)
shows the application guard.

Advance toolkit time, apply game-state changes on the UI owner, then record the
scene. A widget callback may update simulation-owned intent such as
`apply_requested_`; graphics operations belong to the platform owner. Do not run
a toolkit's independent main loop over Cheryl's runtime.

## Publish and clean up

Recording creates owned CPU scene data. Send upload work to the platform through
its dispatcher, consume completed results without blocking simulation, and
append the latest uploaded scene to a prepared frame. The TGUI and RmlUi owner
guides specify their material and scene interfaces. Each frame retains its own
published resources; later UI edits must not mutate an earlier frame's data.

After simulation joins, release application-held widget, document, font and
texture references before session teardown. `close_after_quiescence()` permits
final closure on platform after the UI owner has stopped. Keep input and upload
dependencies alive until accepted work settles. Do not destroy the toolkit while
callbacks or UI operations can still run.

## Continue

Use [the GUI task guide](../getting-started/how-to/create-gui.md) for focused
integration, [GUI system relationships](../engine-systems/gui.md) for ownership,
and [input focus contracts](../runtime/input-state-model.md#focus-routing-and-editing-controls)
for routing detail. Clipboard, cursor, composition and other services depend on
reported integration capabilities; a text field does not imply a complete IME
or editor service. See [UI adapter authoring](../development/ui-adapters.md) when
implementing a different toolkit bridge.
