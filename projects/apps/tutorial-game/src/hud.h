#pragma once

#include <cheryl/assets/submission/draw2d.h>
#include <cheryl/core/engine/engine-context.h>
#include <cheryl/core/rendering/render-frame.h>
#include <cheryl/text/font-collection.h>
#include <cheryl/text/layout.h>

#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <format>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace Tutorial {
    class Hud final {
        CE::Text::FontCollection fonts_;
        std::shared_ptr<const CE::Rendering::Resources::Material> material_;
        CE::Runtime::PlatformDispatcher::Submission platform_;
        std::optional<CE::Text::Rendering::Generation> current_;
        std::future<CE::Text::Rendering::Generation> pending_;
        int requested_ = 0;
        int displayed_ = 0;
        int submitted_ = 0;

    public:
        Hud(CE::Runtime::Context& engine,
            CE::Text::FontCollection fonts,
            std::shared_ptr<const CE::Rendering::Resources::Material> material)
        : fonts_(std::move(fonts)), material_(std::move(material)), platform_(engine.platform_dispatcher().submission()) {
            if (!material_)
                throw std::invalid_argument("HUD requires a text material");
            current_.emplace(CE::Text::Rendering::upload(prepare(0), engine.resources()));
        }

        void set_score(int value) { requested_ = value; }

        void update() {
            if (pending_.valid() && pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto replacement = pending_.get();
                current_.emplace(std::move(replacement));
                displayed_ = submitted_;
            }
            if (!pending_.valid() && requested_ != displayed_) {
                auto prepared = prepare(requested_);
                pending_ = platform_.submit([prepared = std::move(prepared)](CE::Runtime::Context& owner) mutable {
                    return CE::Text::Rendering::upload(std::move(prepared), owner.resources());
                });
                submitted_ = requested_;
            }
        }

        void append(CE::Rendering::PassWriter& pass, glm::vec2 origin) const {
            const CE::Rendering::Submission::Context2D context{
                pass.semantics(), pass.parameters(), pass.constraints(), CE::Rendering::Submission::ImageParameter2D{"image", 0}
            };
            CE::Rendering::DrawStyle2D style;
            style.material = material_;
            style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(origin, 0.0f));
            pass.add(CE::Rendering::Submission::resolve_text(*current_, style, context));
        }

    private:
        [[nodiscard]] CE::Text::Rendering::Prepared prepare(int score) const {
            return CE::Text::Rendering::prepare(CE::Text::layout_text(fonts_, std::format("Score: {}", score), {.pixel_height = 24}));
        }
    };
}
