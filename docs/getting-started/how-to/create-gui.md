# Create an interactive GUI

Add a TGUI button and edit field to the game, route keyboard/pointer input and
publish a retained UI scene. Start with [First Game](../first-game.md). This task
uses native TGUI widgets and Cheryl's existing session/uploader interfaces; the
[RmlUi owner guide](../../modules/rmlui.md) describes the alternative document
authoring path.

## Select the module and prepare materials

Enable/link `Cheryl::UI::TGUI` alongside the native graphics setup. Include:

```cpp
#include <cheryl/backends/opengl/resource-provider.h>
#include <cheryl/ui/tgui/scene.h>
#include <cheryl/ui/tgui/session.h>
#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/EditBox.hpp>
#include <future>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
```

The teaching game's UI material builder requires these shaders under your asset root's
`graphics/shaders/` directory:
[tgui.vert](../../../assets/graphics/shaders/tgui.vert),
[tgui-solid.frag](../../../assets/graphics/shaders/tgui-solid.frag) and
[tgui-textured.frag](../../../assets/graphics/shaders/tgui-textured.frag).
Copy those three files when extending First Game's smaller asset set. This
builder is a manual pipeline path; the asset index does not load it automatically.

Use the maintained teaching game's platform-owned material construction helper:

```{literalinclude} ../../../projects/apps/tutorial-game/src/controls.cpp
:language: cpp
:start-after: // [materials]
:end-before: // [/materials]
```

Put that helper in your game's `.cpp` file before its method definitions. It
uses the selected OpenGL resource provider to create the solid/textured colored
triangle pipelines with compatible blending and sampler semantics. A different
renderer needs its own compatible builder; the session itself has no OpenGL
dependency.

## Retain the integration state

Add these game members. Declare native widget handles after the session so
ordinary member destruction releases the external handles first:

```cpp
CE::Runtime::PlatformDispatcher::Submission ui_platform_;
std::optional<CE::UI::TGUI::SceneUploader> ui_uploader_;
CE::UI::TGUI::Materials ui_materials_;
CE::UI::TGUI::Scene ui_scene_;
std::future<CE::UI::TGUI::Scene> ui_pending_;
std::unique_ptr<CE::UI::TGUI::Session> ui_;
tgui::EditBox::Ptr field_;
bool apply_requested_ = false;
bool ignore_quit_until_release_ = false;
```

During `init()` on platform, after graphics is initialized:

```cpp
ui_platform_ = engine_.platform_dispatcher().submission();
ui_uploader_.emplace(engine_.resources());
ui_materials_ = materials(engine_.resources(), asset_root_);
```

Do not create native toolkit objects there when concurrent mode will update UI
on simulation. The uploader is constructed on its provider's owner; its saved
handle can then be used to submit owned recordings from the UI owner.

## Create native widgets on the first update

After the game's quit handling, initialize the session from the simulation hook:

```cpp
if (!ui_) {
    ui_ = std::make_unique<CE::UI::TGUI::Session>(engine_.input(), 2);
    ui_->set_view(tick.logical_size, tick.framebuffer_size);

    auto button = tgui::Button::create("Apply");
    button->setSize({120, 32});
    button->setPosition({16, 16});
    button->onPress([this] { apply_requested_ = true; });
    ui_->gui().add(button);

    field_ = tgui::EditBox::create();
    field_->setSize({240, 32});
    field_->setPosition({16, 60});
    field_->setDefaultText("Type a name");
    ui_->gui().add(field_);
    ui_->request_keyboard_focus();
    field_->setFocused(true);
} else {
    ui_->set_view(tick.logical_size, tick.framebuffer_size);
}
```

Target `2` is a chosen nonzero routing identity. Reserve distinct IDs for other
sessions in the application. Session construction requests Events capture;
keyboard focus requests Text capture. The widget's own focused state and the
session's routed keyboard ownership are related but separate.

The session and all native toolkit objects remain on this UI owner. The
`engine_.input()` getter borrows the adapter; it does not poll or mutate bindings.
Keep that adapter alive through the session's final closure.

## Feed records and control focus

At the beginning of `update()`, guard the existing quit check before creating
widgets or processing records:

```cpp
if (tick.input.button(quit_).pressed() && !ignore_quit_until_release_) {
    tick.request_stop();
    return;
}
```

After creating/updating the session, feed the complete ordered record batch:

```cpp
ui_->handle_input(tick.input.records(), true, [&](const CE::Input::Events::Record& record) {
    const auto* button = std::get_if<CE::Input::Events::Button>(&record.data);
    if (button && record.device_kind == CE::Input::DeviceKind::Keyboard) {
        if (button->key == CE::Input::KeyboardKey::Escape) {
            if (button->phase == CE::Input::Events::ButtonPhase::Press && ui_->owns_keyboard_focus()) {
                ignore_quit_until_release_ = true;
                ui_->release_keyboard_focus();
            } else if (button->phase == CE::Input::Events::ButtonPhase::Release)
                ignore_quit_until_release_ = false;
        } else if (button->key == CE::Input::KeyboardKey::F2 &&
                   button->phase == CE::Input::Events::ButtonPhase::Press) {
            ui_->request_keyboard_focus();
            field_->setFocused(true);
        }
    }
    return true;
});
ui_->update_time(tick.delta_seconds);
if (std::exchange(apply_requested_, false))
    field_->setText("Applied");
```

The `true` values explicitly give this visible interface pointer delivery for
the task. A real overlay must decide hit testing and gameplay pointer ownership;
exclusive keyboard focus does not automatically capture/gate mouse gameplay.
Feed the whole batch once, including releases needed by modifier tracking.

Escape releases routed focus and F2 requests it again. Requests take effect on
later platform polls; the session retains its entry epoch for the current batch
rather than relabeling records. While exclusive focus is active, First Game's
keyboard quit/movement actions are gated. When focus ends, a still-held key can
resume as a semantic press. Keep the game's quit check before this record handling
and require `!ignore_quit_until_release_` in addition to the quit action's
`pressed()` result. The releasing Escape stays suppressed through its physical
release batch; a later Escape press can reach the ordinary quit action. The
maintained Controls class exposes this decision through `permits_quit()`.

## Upload without queue growth and publish

At the end of update:

```cpp
static_cast<void>(CE::UI::TGUI::adopt_scene(ui_pending_, ui_scene_));
if (!ui_pending_.valid())
    ui_pending_ = ui_uploader_->submit(ui_platform_, ui_->record(), ui_materials_);
```

Adoption does not wait on pending work. Keep the previous complete scene while
one replacement waits; only owned recording/material data crosses to graphics,
never live widgets. Ready upload errors propagate here for the game's selected
failure policy. The maintained demo catches them to retain/report its previous
scene; allowing propagation uses runtime's normal failure cleanup.

After your world/HUD passes in `prepare_render_frame()`:

```cpp
ui_scene_.write(frame);
```

This writes retained packet values, not native toolkit operations. The first
scene may be empty until upload completes. Authored pass order puts the UI after
the world. Runtime publishes and renders it without accessing your live widget
tree.

## Close after UI quiescence

In `deinit()`, after simulation has joined and accepted platform dependencies
settled:

```cpp
field_.reset();
if (ui_) {
    ui_->close_after_quiescence();
    ui_.reset();
}
ui_scene_ = CE::UI::TGUI::Scene{};
ui_uploader_.reset();
ui_materials_ = CE::UI::TGUI::Materials{};
```

Keep the game's other partial-safe cleanup. Drop every application-held native
widget/font/texture reference before session closure; the GUI releases its own
hierarchy during shutdown. Do not race closure with input, callbacks or UI
mutation. Retained Cheryl scenes can outlive toolkit objects while their
graphics domain remains live.

The field should accept committed text, arrows and Backspace while focused.
Clicking Apply should display `Applied`; Escape releases editing and F2 restores
it. Resizing should update layout/view dimensions, and both runtime modes should
preserve input and teardown boundaries. Clipboard, cursor and IME services remain
limited by reported capabilities, regardless of what widgets normally support
on another backend.

Read [GUI construction](../../gui/index.md),
[TGUI contracts](../../modules/tgui.md),
[focus routing](../../runtime/input-state-model.md#focus-routing-and-editing-controls)
and [the maintained Controls implementation](../../../projects/apps/tutorial-game/src/controls.cpp)
for the complete maintained task. [The larger demo](../../../projects/apps/demo/src/tgui-demo.cpp)
also illustrates layout, pointer hit testing, coexistence and recoverable uploads.
