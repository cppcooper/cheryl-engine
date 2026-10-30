#pragma once

#include "hud.h"

#include <cheryl/core/engine/engine-context.h>
#include <cheryl/core/game-framework/tick-context.h>
#include <cheryl/ui/tgui/scene.h>
#include <cheryl/ui/tgui/session.h>

#include <future>
#include <memory>
#include <optional>

namespace Tutorial {
    class UIController final {
        static constexpr CE::Input::Routing::TargetId tgui_target_ = 2;

        CE::Runtime::Context& engine_;
        CE::Runtime::PlatformDispatcher::Submission platform_;
        CE::UI::TGUI::SceneUploader uploader_;
        CE::UI::TGUI::Scene scene_;
        std::future<CE::UI::TGUI::Scene> pending_;
        std::unique_ptr<CE::UI::TGUI::Session> session_;
        std::optional<Hud> hud_;
        int score_ = 0;
        unsigned int health_ = 100;

    public:
        // Construct during Game::init_platform(); create toolkit objects on simulation.
        explicit UIController(CE::Runtime::Context& engine)
        : engine_(engine), platform_(engine.platform_dispatcher().submission()), uploader_(engine.resources()) {}

        void set_score(int value) { score_ = value; }
        void set_health(unsigned int value) { health_ = value; }

        void init_simulation() {
            session_ = std::make_unique<CE::UI::TGUI::Session>(engine_, tgui_target_, CE::UI::TGUI::SessionOptions{.automatic_view = true});
            hud_.emplace(*session_);
        }

        void update(const CE::Game::TickContext& tick) {
            // This example has a passive HUD. Interactive components choose focus/pointer policy here.
            session_->handle_input(tick.input.records(), false);
            session_->update_time(tick.delta_seconds);
            hud_->update(score_, health_);

            static_cast<void>(CE::UI::TGUI::adopt_scene(pending_, scene_));
            if (!pending_.valid())
                pending_ = uploader_.submit(platform_, session_->record());
        }

        void append(CE::Rendering::FrameWriter& frame) const { scene_.write(frame); }

        void close_after_quiescence() {
            // Drop component-held toolkit references before the GUI/session releases its own.
            hud_.reset();
            if (session_) {
                session_->close_after_quiescence();
                session_.reset();
            }
            scene_ = CE::UI::TGUI::Scene{};
        }
    };
}
