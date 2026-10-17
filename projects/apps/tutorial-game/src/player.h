#pragma once

#include <cheryl/assets/submission/draw2d.h>
#include <cheryl/core/game-framework/tick-context.h>
#include <cheryl/core/rendering/render-frame.h>

#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <stdexcept>
#include <utility>

namespace Tutorial {
    class Player final {
        std::shared_ptr<const CE::Assets::Sprites::Asset> sprite_;
        std::shared_ptr<const CE::Rendering::Resources::Material> material_;
        glm::vec2 position_;
        CE::Input::Actions::Id left_;
        CE::Input::Actions::Id right_;
        float speed_ = 160.0f;

    public:
        Player(
            std::shared_ptr<const CE::Assets::Sprites::Asset> sprite,
            std::shared_ptr<const CE::Rendering::Resources::Material> material,
            glm::vec2 position,
            CE::Input::Actions::Id left,
            CE::Input::Actions::Id right
        )
        : sprite_(std::move(sprite)), material_(std::move(material)), position_(position), left_(left), right_(right) {
            if (!sprite_ || !material_)
                throw std::invalid_argument("Player requires a sprite and material");
        }

        void update(const CE::Game::TickContext& tick) {
            const float direction = (tick.input.button(right_).held() ? 1.0f : 0.0f) - (tick.input.button(left_).held() ? 1.0f : 0.0f);
            position_.x += direction * speed_ * static_cast<float>(tick.delta_seconds);
        }

        void append(CE::Rendering::PassWriter& pass) const {
            const CE::Rendering::Submission::Context2D context{
                pass.semantics(), pass.parameters(), pass.constraints(), CE::Rendering::Submission::ImageParameter2D{"image", 0}
            };
            CE::Rendering::DrawStyle2D style;
            style.material = material_;
            style.scale = 4.0f;
            style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(position_, 0.0f));
            pass.add(CE::Rendering::Submission::resolve_sprite(*sprite_, 0, style, context));
        }

        [[nodiscard]] glm::vec2 position() const { return position_; }
    };
}
