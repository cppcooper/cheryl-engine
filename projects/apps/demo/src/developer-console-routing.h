#pragma once

#include <cheryl/core/controls/input-record.h>

#include <memory>
#include <span>

namespace Rml {
    class ElementDocument;
}
namespace CE::UI::RmlUi {
    class Session;
}
class DemoTguiControls;
namespace DemoConsole {
    class View;

    class Routing final {
        struct State;
        std::unique_ptr<State> state_;

    public:
        Routing(CE::UI::RmlUi::Session& session, Rml::ElementDocument& original, View* console);
        ~Routing();
        Routing(const Routing&) = delete;
        Routing& operator=(const Routing&) = delete;
        // Drains each Session once; acquires new focus only after both entry epochs drain.
        bool handle_input(std::span<const CE::Input::Events::Record> records, bool drawable, DemoTguiControls* tgui = nullptr);
        bool consume_reset() noexcept;
        bool original_visible() const noexcept;
        void retire();
    };
}
