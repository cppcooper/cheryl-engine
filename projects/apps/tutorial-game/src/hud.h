#pragma once

#include <cheryl/core/engine/engine-context.h>
#include <cheryl/core/game-framework/tick-context.h>
#include <cheryl/ui/tgui/scene.h>
#include <cheryl/ui/tgui/session.h>

#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/ProgressBar.hpp>

#include <algorithm>
#include <format>
#include <future>
#include <memory>

namespace Tutorial {
    class Hud final {
        CE::Runtime::Context& engine_;
        CE::Runtime::PlatformDispatcher::Submission platform_;
        CE::UI::TGUI::SceneUploader uploader_;
        CE::UI::TGUI::Scene scene_;
        std::future<CE::UI::TGUI::Scene> pending_;
        std::unique_ptr<CE::UI::TGUI::Session> session_;
        tgui::Label::Ptr score_;
        tgui::ProgressBar::Ptr health_;
        int score_value_ = 0;
        unsigned int health_value_ = 100;

    public:
        explicit Hud(CE::Runtime::Context& engine)
        : engine_(engine), platform_(engine.platform_dispatcher().submission()), uploader_(engine.resources()) {}

        void set_score(int value) { score_value_ = value; }
        void set_health(unsigned int value) { health_value_ = std::min(value, 100u); }

        void update(const CE::Game::TickContext& tick) {
            if (!session_) {
                session_ = std::make_unique<CE::UI::TGUI::Session>(engine_, 2);
                session_->set_view(tick.logical_size, tick.framebuffer_size);
                create_widgets();
            } else
                session_->set_view(tick.logical_size, tick.framebuffer_size);

            session_->handle_input(tick.input.records(), false);
            session_->update_time(tick.delta_seconds);
            score_->setText(std::format("Score: {}", score_value_));
            health_->setValue(health_value_);
            health_->setText(std::format("Health: {}/100", health_value_));

            static_cast<void>(CE::UI::TGUI::adopt_scene(pending_, scene_));
            if (!pending_.valid())
                pending_ = uploader_.submit(platform_, session_->record());
        }

        void append(CE::Rendering::FrameWriter& frame) const { scene_.write(frame); }

        void close_after_quiescence() {
            score_.reset();
            health_.reset();
            if (session_) {
                session_->close_after_quiescence();
                session_.reset();
            }
            scene_ = CE::UI::TGUI::Scene{};
        }

    private:
        void create_widgets() {
            score_ = tgui::Label::create("Score: 0");
            score_->setTextSize(24);
            score_->setSize({220, 32});
            session_->gui().add(score_);
            session_->set_layout(
                score_, {
                            .anchor = {0, 0},
                            .offset = {.fixed = {24, 16}},
                        }
            );

            health_ = tgui::ProgressBar::create();
            health_->setSize({220, 32});
            health_->setMaximum(100);
            session_->gui().add(health_);
            session_->set_layout(
                health_, {
                             .anchor = {1, 0},
                             .offset = {.fixed = {-24, 16}},
                         }
            );
        }
    };
}
