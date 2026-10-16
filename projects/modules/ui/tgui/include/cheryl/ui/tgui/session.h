#pragma once

#include <cheryl/export.h>

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
        CE_UI_TGUI_API Session(CE::Input::System& input, CE::Input::Routing::TargetId target, SessionOptions options = {});
        CE_UI_TGUI_API ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        [[nodiscard]] CE_UI_TGUI_API tgui::BackendGui& gui();
        [[nodiscard]] CE_UI_TGUI_API Capabilities capabilities() const;
        // Both dimensions are copied by the runtime; no native window is read.
        CE_UI_TGUI_API void set_view(CE::Platform::Dimensions<int> logical, CE::Platform::FramebufferSize framebuffer);
        // Add the widget to this GUI first. Typed rules bind its current parent's
        // content dimensions; reapply after reparenting. Released scalable axes
        // freeze at their current size. Other native sizing remains unchanged.
        CE_UI_TGUI_API void set_layout(tgui::Widget::Ptr widget, const WidgetLayout& layout);
        CE_UI_TGUI_API void update_time(double seconds);
        CE_UI_TGUI_API void request_keyboard_focus(CE::Input::Routing::KeyboardPolicy routing = CE::Input::Routing::KeyboardPolicy::Exclusive);
        CE_UI_TGUI_API void release_keyboard_focus();
        [[nodiscard]] CE_UI_TGUI_API bool owns_keyboard_focus() const;
        // Observe the entire ordered stream for modifiers, including releases.
        // Deliver keyboard/text only for this session's requested focus epoch.
        // The application explicitly selects pointer delivery for this call.
        CE_UI_TGUI_API void handle_input(std::span<const CE::Input::Events::Record> records, bool pointer_selected);
        // Run application controls before each record and return its pointer
        // selection. Keyboard/text retain the entry epoch throughout the batch;
        // preempted focus is released only after all records have been handled.
        CE_UI_TGUI_API void handle_input(
            std::span<const CE::Input::Events::Record> records,
            bool pointer_selected,
            const std::function<bool(const CE::Input::Events::Record&)>& before_record
        );
        [[nodiscard]] CE_UI_TGUI_API RecordedScene record();

        // After the runtime joins simulation, final teardown may run on platform.
        // Release external toolkit references first; no calls may race teardown.
        // Uploaded Cheryl scenes/frames are independent and may remain retained.
        CE_UI_TGUI_API void close_after_quiescence();

    private:
        [[nodiscard]] CE_UI_TGUI_API State& owner() const;
    };
}
