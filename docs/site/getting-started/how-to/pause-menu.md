# Combine a HUD and pause menu

Keep a TGUI HUD visible while an RmlUi menu pauses the world. One application
UI owner coordinates two toolkit sessions; the game still chooses which world
updates run. Native button callbacks record requests, and the game applies them
after toolkit input returns.

Start with [the TGUI HUD](../create-a/hud.md), including its `hud.h`, and
[the RmlUi module setup](create-gui.md#select-the-module-and-prepare-assets).
Enable both modules and link `Cheryl::UI::TGUI` and `Cheryl::UI::RmlUi` to the game.
This replaces the preceding `Tutorial::UIController`; it does not create a second
TGUI session. Keep First Game's graphics loading, world drawing and camera updates.

## Add a menu document

Create `graphics/ui/pause.rml` under the existing asset root:

```xml
<rml>
<head>
    <title>Paused</title>
    <style>
        body { width: 100%; height: 100%; margin: 0; color: #ffffff; font-size: 22px; }
        #panel { position: absolute; left: 25%; top: 25%; width: 50%;
                 padding: 20px; background-color: #26394bee; }
        button { display: block; padding: 10px; margin: 12px 0; background-color: #477989; }
        button:hover { background-color: #5b92a0; }
    </style>
</head>
<body>
    <div id="panel">
        <h1>Paused</h1>
        <button id="resume">Resume</button>
        <button id="quit">Quit</button>
    </div>
</body>
</rml>
```

The document inherits the configured Engine font. Its initial state is hidden;
the controller shows it when Escape is pressed. This example reserves Escape
for the menu: remove the game's old Escape-to-quit registration and update branch.
Quit now comes from the menu. Keep unrelated movement and score bindings.

## Give both sessions one application owner

Create `mixed-ui-controller.h` beside `hud.h`. These are application classes,
not new Cheryl interfaces:

```cpp
#pragma once

#include "hud.h"
#include <cheryl/core/controls/button-codes.h>
#include <cheryl/core/engine/engine-context.h>
#include <cheryl/core/engine/platform-dispatcher.h>
#include <cheryl/core/game-framework/tick-context.h>
#include <cheryl/core/rendering/render-frame.h>
#include <cheryl/ui/rmlui/scene.h>
#include <cheryl/ui/rmlui/session.h>
#include <cheryl/ui/tgui/scene.h>
#include <cheryl/ui/tgui/session.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace Tutorial {
    class MenuFlagListener final : public Rml::EventListener {
        bool& requested_;
    public:
        explicit MenuFlagListener(bool& requested) : requested_(requested) {}
        void ProcessEvent(Rml::Event&) override { requested_ = true; }
    };

    class MixedUIController final {
        CE::Runtime::Context& engine_;
        std::filesystem::path asset_root_;
        std::optional<CE::Runtime::PlatformDispatcher::Submission> platform_;
        std::optional<CE::UI::TGUI::SceneUploader> hud_uploader_;
        std::optional<CE::UI::RmlUi::SceneUploader> menu_uploader_;
        CE::UI::TGUI::Scene hud_scene_;
        CE::UI::RmlUi::Scene menu_scene_;
        std::future<CE::UI::TGUI::Scene> hud_pending_;
        std::future<CE::UI::RmlUi::Scene> menu_pending_;
        std::unique_ptr<CE::UI::TGUI::Session> hud_session_;
        std::unique_ptr<CE::UI::RmlUi::Session> menu_session_;
        std::optional<Hud> hud_;
        Rml::ElementDocument* document_ = nullptr;
        Rml::Element* resume_ = nullptr;
        Rml::Element* quit_ = nullptr;
        bool menu_open_ = false;
        bool toggle_requested_ = false;
        bool resume_requested_ = false;
        bool quit_requested_ = false;
        std::unique_ptr<MenuFlagListener> resume_listener_;
        std::unique_ptr<MenuFlagListener> quit_listener_;

    public:
        MixedUIController(CE::Runtime::Context& engine, std::filesystem::path asset_root)
        : engine_(engine), asset_root_(std::move(asset_root)) {}

        void init_platform() {
            platform_.emplace(engine_.platform_dispatcher().submission());
            hud_uploader_.emplace(engine_.resources());
            menu_uploader_.emplace(engine_.resources());
        }

        void init_simulation() {
            constexpr CE::Input::Routing::TargetId hud_target = 2, menu_target = 3;
            hud_session_ = std::make_unique<CE::UI::TGUI::Session>(
                engine_, hud_target, CE::UI::TGUI::SessionOptions{.automatic_view = true});
            menu_session_ = std::make_unique<CE::UI::RmlUi::Session>(
                engine_, menu_target, CE::UI::RmlUi::SessionOptions{.automatic_view = true});
            hud_.emplace(*hud_session_);
            const auto source = (asset_root_ / "graphics/ui/pause.rml").u8string();
            document_ = menu_session_->context().LoadDocument(Rml::String(source.begin(), source.end()));
            if (!document_)
                throw std::runtime_error("Cannot load the pause menu");
            resume_ = document_->GetElementById("resume");
            quit_ = document_->GetElementById("quit");
            if (!resume_ || !quit_)
                throw std::runtime_error("Pause menu requires Resume and Quit buttons");
            resume_listener_ = std::make_unique<MenuFlagListener>(resume_requested_);
            quit_listener_ = std::make_unique<MenuFlagListener>(quit_requested_);
            resume_->AddEventListener("click", resume_listener_.get());
            quit_->AddEventListener("click", quit_listener_.get());
            document_->Hide();
        }

        [[nodiscard]] bool paused() const { return menu_open_; }

        bool begin_update(const CE::Game::TickContext& tick) {
            hud_session_->handle_input(tick.input.records(), false);
            menu_session_->handle_input(tick.input.records(), menu_open_,
                [&](const CE::Input::Events::Record& record) {
                    const auto* button = std::get_if<CE::Input::Events::Button>(&record.data);
                    if (button && record.device_kind == CE::Input::DeviceKind::Keyboard &&
                        button->key == CE::Input::KeyboardKey::Escape &&
                        button->phase == CE::Input::Events::ButtonPhase::Press)
                        toggle_requested_ = !toggle_requested_;
                    return menu_open_;
                });

            bool next_open = menu_open_;
            if (std::exchange(toggle_requested_, false))
                next_open = !next_open;
            if (std::exchange(resume_requested_, false))
                next_open = false;
            if (next_open != menu_open_) {
                menu_open_ = next_open;
                if (menu_open_) {
                    document_->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
                    menu_session_->request_keyboard_focus(CE::Input::Routing::KeyboardPolicy::PassThrough);
                    if (!resume_->Focus())
                        throw std::runtime_error("Cannot focus Resume");
                } else {
                    menu_session_->release_keyboard_focus();
                    document_->Hide();
                }
            }
            hud_session_->update_time(tick.delta_seconds);
            menu_session_->update_time(tick.delta_seconds);
            static_cast<void>(CE::UI::TGUI::adopt_scene(hud_pending_, hud_scene_));
            static_cast<void>(CE::UI::RmlUi::adopt_scene(menu_pending_, menu_scene_));
            return std::exchange(quit_requested_, false);
        }

        void publish(int score, unsigned int health) {
            hud_->update(score, health);
            if (!hud_pending_.valid())
                hud_pending_ = hud_uploader_->submit(*platform_, hud_session_->record());
            if (menu_open_ && !menu_pending_.valid())
                menu_pending_ = menu_uploader_->submit(*platform_, menu_session_->record());
        }

        void append(CE::Rendering::FrameWriter& frame) const {
            hud_scene_.write(frame);
            if (menu_open_)
                menu_scene_.write(frame);
        }

        void close_after_quiescence() {
            if (resume_ && resume_listener_)
                resume_->RemoveEventListener("click", resume_listener_.get());
            if (quit_ && quit_listener_)
                quit_->RemoveEventListener("click", quit_listener_.get());
            resume_listener_.reset();
            quit_listener_.reset();
            document_ = nullptr;
            resume_ = quit_ = nullptr;
            hud_.reset();
            if (menu_session_)
                menu_session_->close_after_quiescence();
            if (hud_session_)
                hud_session_->close_after_quiescence();
            menu_session_.reset();
            hud_session_.reset();
            menu_pending_ = {};
            hud_pending_ = {};
            menu_scene_ = {};
            hud_scene_ = {};
            menu_uploader_.reset();
            hud_uploader_.reset();
            platform_.reset();
        }
    };
}
```

Uploaders capture their creating thread: constructing them on simulation would
make dispatched platform uploads throw `failed_operation`, rethrown when the
future is adopted. Sessions instead capture the simulation/UI owner; live
session calls from another thread reject. A single C++ owner coordinates these
objects without making their execution owners interchangeable.

Each toolkit gets one distinct nonzero `TargetId`. The HUD never requests
keyboard focus and selects no pointer delivery. The menu requests session focus
and separately focuses its native Resume element. `PassThrough` lets its controls
receive keyboard input while the game gates world updates explicitly. Choose
`Exclusive` for text entry when semantic keyboard gameplay actions must also be
withheld; Escape still works here because the application observes raw records
before toolkit routing. Keyboard focus alone does not gate mouse or gamepad play.

Both sessions receive the complete ordered batch once, including empty batches
and modifier releases. Menu toggles and button requests are applied afterward,
so opening a hidden menu admits pointer input starting with the next batch.
Focus requests likewise affect future polls; they do not relabel existing input.

## Put the game updates around UI input

Include `mixed-ui-controller.h` in the game header and replace the old `ui_`
member with `std::optional<Tutorial::MixedUIController> ui_;`. Use these hooks:

| Hook | Work |
| --- | --- |
| `init_platform()` | After graphics/font setup, call `ui_.emplace(engine_, asset_root_); ui_->init_platform();`. Acquire any application action endpoints and native device IDs here too; saved endpoints can later attach contexts from simulation. |
| `init_simulation()` | Call `ui_->init_simulation()` to construct both sessions, HUD, document and listeners. |
| `update(tick)` | Use the update block below. Keep view/camera observation and other always-running services outside its world branch. |
| `prepare_render_frame(frame)` | Keep world draws, then call `ui_->append(frame)`, which appends HUD then menu passes. |
| `deinit_simulation()` | Stop application UI/world producers while their owner is still available, if any. This example has no additional producers. Final toolkit teardown waits for platform cleanup. |
| `deinit_platform()` | After simulation joins and accepted uploads settle, call `if (ui_) ui_->close_after_quiescence();`, then `ui_.reset()`, before releasing graphics/input dependencies. |

Replace the game's UI/gameplay portion of `update()` with this structure, moving
its existing world update statements into the indicated branch. `points_` and
`health_` are the values from the HUD lesson:

```cpp
const bool was_paused = ui_->paused();
if (ui_->begin_update(tick)) {
    tick.request_stop();
    return;
}
if (!was_paused && !ui_->paused()) {
    // Put existing player, AI, movement, collision and world-animation updates here.
}
ui_->publish(points_, health_);
```

Pause is game state, not a runtime stop. Keep frame preparation, UI input, toolkit
time and scene publication running while the world branch is skipped. This
example skips gameplay on both the opening and closing tick, so a Resume click
cannot also fire a weapon in that tick. A held control can still become active
on a later poll; games needing release-before-reuse should keep a separate
physical-release guard. Apply the same explicit gate to gamepad and mouse actions.
Retained audio plays on its own device clock; apply a separate
[audio pause policy](../../runtime/audio.md#lifetime-and-scheduling).

For a New Game button, add another request flag/listener and apply it in the
game after `begin_update()` returns, before touching old actors or iterating the
new world. Do not reset the controller or destroy its document from its own click
callback. Application events can carry that intent too; their
[immediate delivery](../create-a/event.md#defer-destructive-game-transitions)
still runs inside the publisher unless an adapter was selected.

## Keep presentation and cleanup predictable

Each toolkit has one outstanding upload, its own future type, and its own retained
scene. Adoption checks readiness without blocking simulation and propagates ready
upload errors. The first menu opening may be blank until upload completes. Later
openings can briefly show its previous complete scene; this static menu permits
that. `append()` gates menu visibility immediately, even while an old upload is
pending. Native `Hide()` by itself cannot hide already retained packets.

For changing pages or scores that must never reopen stale, use
[revision filtering at adoption](../../gui/index.md#publish-and-clean-up). Clearing
only the retained scene is insufficient while an older completion can restore it.

The cleanup method supports partial initialization. Release native listener and
widget aliases before closing sessions, after simulation has joined; leave the
provider/input/dispatcher alive until accepted uploads and cleanup settle.
CPU recordings and uploaded scenes retain their own resources independently.

Expect Escape to show the menu over the HUD and freeze the world. Resume or
Escape closes it; world updates resume on a later tick. Menu hover/focus and UI
publication continue while paused, and Quit requests normal runtime shutdown.
Those are the observations to check in sequential and concurrent modes.
