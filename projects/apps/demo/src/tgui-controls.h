#pragma once

#include "ui-status.h"

#include <core/controls/input-record.h>

#include <memory>
#include <span>

namespace CE::UI::TGUI {
    class Session;
}

// The original demo widgets and their deferred effects, below one borrowed Session.
class DemoTguiControls final {
    struct State;
    std::unique_ptr<State> state_;

public:
    explicit DemoTguiControls(CE::UI::TGUI::Session& session);
    ~DemoTguiControls();
    DemoTguiControls(const DemoTguiControls&) = delete;
    DemoTguiControls& operator=(const DemoTguiControls&) = delete;
    // Detach on the Session owner; repeats after a serial handoff are inert.
    void retire();
    CE::UI::TGUI::Session& session() const;
    bool visible() const noexcept;
    void set_visible(bool visible);
    bool contains(const CE::Input::Events::Pointer& position) const;
    bool field_contains(const CE::Input::Events::Pointer& position) const;
    void focus_field();
    void update_status(const DemoUiStatus& status);
    void begin_input(bool enabled);
    void select_effects(bool selected);
    void cancel_effects();
    void finish_input();
    bool consume_reset() noexcept;
    bool alternate_image() const noexcept;
    void handle_input(std::span<const CE::Input::Events::Record> records, bool drawable);
};
