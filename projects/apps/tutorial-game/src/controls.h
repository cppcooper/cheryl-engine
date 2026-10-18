#pragma once

#include <cheryl/core/game-framework/tick-context.h>
#include <cheryl/ui/tgui/scene.h>
#include <cheryl/ui/tgui/session.h>

#include <TGUI/Widgets/EditBox.hpp>

#include <future>
#include <memory>

namespace Tutorial {
    class Controls final {
        CE::Input::System& input_;
        CE::Runtime::PlatformDispatcher::Submission platform_;
        CE::UI::TGUI::SceneUploader uploader_;
        CE::UI::TGUI::Scene scene_;
        std::future<CE::UI::TGUI::Scene> pending_;
        std::unique_ptr<CE::UI::TGUI::Session> session_;
        tgui::EditBox::Ptr field_;
        bool apply_requested_ = false;
        bool ignore_quit_until_release_ = false;

    public:
        explicit Controls(CE::Runtime::Context& engine);
        [[nodiscard]] bool permits_quit() const { return !ignore_quit_until_release_; }
        bool update(const CE::Game::TickContext& tick);
        void write(CE::Rendering::FrameWriter& frame) const;
        void close_after_quiescence();

    private:
        void create_widgets(const CE::Game::TickContext& tick);
    };
}
