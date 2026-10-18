#include "controls.h"

#include <cheryl/core/engine/engine-context.h>

#include <TGUI/Widgets/Button.hpp>

#include <utility>
#include <variant>

namespace Tutorial {
    Controls::Controls(CE::Runtime::Context& engine)
    : input_(engine.input()), platform_(engine.platform_dispatcher().submission()), uploader_(engine.resources()) {}

    // [widgets]
    void Controls::create_widgets(const CE::Game::TickContext& tick) {
        session_ = std::make_unique<CE::UI::TGUI::Session>(input_, 2);
        session_->set_view(tick.logical_size, tick.framebuffer_size);

        auto button = tgui::Button::create("Apply");
        button->setSize({120, 32});
        button->setPosition({16, 80});
        button->onPress([this] { apply_requested_ = true; });
        session_->gui().add(button);

        field_ = tgui::EditBox::create();
        field_->setSize({240, 32});
        field_->setPosition({16, 124});
        field_->setDefaultText("Type a name");
        session_->gui().add(field_);
        session_->request_keyboard_focus();
        field_->setFocused(true);
    }
    // [/widgets]

    // [update]
    bool Controls::update(const CE::Game::TickContext& tick) {
        if (!session_)
            create_widgets(tick);
        else
            session_->set_view(tick.logical_size, tick.framebuffer_size);

        session_->handle_input(tick.input.records(), true, [&](const CE::Input::Events::Record& record) {
            const auto* button = std::get_if<CE::Input::Events::Button>(&record.data);
            if (button && record.device_kind == CE::Input::DeviceKind::Keyboard) {
                if (button->key == CE::Input::KeyboardKey::Escape) {
                    if (button->phase == CE::Input::Events::ButtonPhase::Press && session_->owns_keyboard_focus()) {
                        ignore_quit_until_release_ = true;
                        session_->release_keyboard_focus();
                    } else if (button->phase == CE::Input::Events::ButtonPhase::Release)
                        ignore_quit_until_release_ = false;
                } else if (button->key == CE::Input::KeyboardKey::F2 && button->phase == CE::Input::Events::ButtonPhase::Press) {
                    session_->request_keyboard_focus();
                    field_->setFocused(true);
                }
            }
            return true;
        });
        session_->update_time(tick.delta_seconds);
        const bool applied = std::exchange(apply_requested_, false);
        if (applied)
            field_->setText("Applied");

        static_cast<void>(CE::UI::TGUI::adopt_scene(pending_, scene_));
        if (!pending_.valid())
            pending_ = uploader_.submit(platform_, session_->record());
        return applied;
    }
    // [/update]

    void Controls::write(CE::Rendering::FrameWriter& frame) const {
        scene_.write(frame);
    }

    // [cleanup]
    void Controls::close_after_quiescence() {
        field_.reset();
        if (session_) {
            session_->close_after_quiescence();
            session_.reset();
        }
        scene_ = CE::UI::TGUI::Scene{};
    }
    // [/cleanup]
}
