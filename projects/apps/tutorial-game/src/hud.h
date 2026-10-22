#pragma once

#include <cheryl/ui/tgui/session.h>

#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/ProgressBar.hpp>

#include <algorithm>
#include <format>

namespace Tutorial {
    class Hud final {
        CE::UI::TGUI::Session& session_;
        tgui::Label::Ptr score_;
        tgui::ProgressBar::Ptr health_;

    public:
        explicit Hud(CE::UI::TGUI::Session& session)
        : session_(session) {
            create_widgets();
        }

        void update(int score, unsigned int health) {
            health = std::min(health, 100u);
            score_->setText(std::format("Score: {}", score));
            health_->setValue(health);
            health_->setText(std::format("Health: {}/100", health));
        }

        // Remove this component during play, on the session's UI owner.
        // Release the Hud afterwards; the shared session remains usable.
        void remove() {
            auto& gui = session_.gui();
            static_cast<void>(gui.remove(score_));
            static_cast<void>(gui.remove(health_));
        }

    private:
        void create_widgets() {
            score_ = tgui::Label::create("Score: 0");
            score_->setTextSize(24);
            score_->setSize({220, 32});
            session_.gui().add(score_);
            session_.set_layout(
                score_, {
                            .anchor = {0, 0},
                            .offset = {.fixed = {24, 16}},
                        }
            );

            health_ = tgui::ProgressBar::create();
            health_->setSize({220, 32});
            health_->setMaximum(100);
            session_.gui().add(health_);
            session_.set_layout(
                health_, {
                             .anchor = {1, 0},
                             .offset = {.fixed = {-24, 16}},
                         }
            );
        }
    };
}
