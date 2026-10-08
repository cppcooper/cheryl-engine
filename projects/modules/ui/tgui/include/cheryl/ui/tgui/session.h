#pragma once

#include "layout.h"
#include "rendering.h"

#include <core/controls/input-interface.h>
#include <core/display/framebuffer-size.h>
#include <core/display/viewport.h>

#include <TGUI/Backend/Window/BackendGui.hpp>

#include <functional>
#include <memory>
#include <span>

namespace CE::UI::TGUI {
    struct SessionOptions {
        unsigned int maximum_texture_size = 4096;
        float font_scale = 1;
    };

    struct Capabilities {
        bool keyboard_focus = false;
        bool committed_text = false;
        bool clipboard = false;
        bool cursor = false;
        bool ime = false;
    };

    // Create on the simulation/UI owner, before creating any toolkit objects.
    // TGUI's process-global backend permits one session at a time. input must
    // outlive the session; widget/font/texture references must not outlive it.
    class Session final {
        struct State;
        std::unique_ptr<State> state_;

    public:
        Session(Input::iInputSystem& input, Input::FocusId target, SessionOptions options = {});
        ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        [[nodiscard]] tgui::BackendGui& gui();
        [[nodiscard]] Capabilities capabilities() const;
        // Both dimensions are copied by the runtime; no native window is read.
        void set_view(ViewPort<int> logical, FramebufferSize framebuffer);
        // Add the widget to this GUI first. Typed rules bind its current parent's
        // content dimensions; reapply after reparenting. Released scalable axes
        // freeze at their current size. Other native sizing remains unchanged.
        void set_layout(tgui::Widget::Ptr widget, const WidgetLayout& layout);
        void update_time(double seconds);
        void request_keyboard_focus(Input::KeyboardRouting routing = Input::KeyboardRouting::Exclusive);
        void release_keyboard_focus();
        [[nodiscard]] bool owns_keyboard_focus() const;
        // Observe the entire ordered stream for modifiers, including releases.
        // Deliver keyboard/text only for this session's requested focus epoch.
        // The application explicitly selects pointer delivery for this call.
        void handle_input(std::span<const Input::InputRecord> records, bool pointer_selected);
        // Run application controls before each record and return its pointer
        // selection. Keyboard/text retain the entry epoch throughout the batch;
        // preempted focus is released only after all records have been handled.
        void handle_input(
            std::span<const Input::InputRecord> records,
            bool pointer_selected,
            const std::function<bool(const Input::InputRecord&)>& before_record
        );
        [[nodiscard]] RecordedScene record();

        // After the runtime joins simulation, final teardown may run on platform.
        // Release external toolkit references first; no calls may race teardown.
        // Uploaded Cheryl scenes/frames are independent and may remain retained.
        void close_after_quiescence();

    private:
        [[nodiscard]] State& owner() const;
    };
}
