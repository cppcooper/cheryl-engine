#pragma once

#include "character-command.h"

#include <cheryl/assets/submission/draw2d.h>
#include <cheryl/core/rendering/render-frame.h>

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace Tutorial {
    class Character final {
        std::shared_ptr<const CE::Assets::Sprites::Asset> sprite_;
        std::shared_ptr<const CE::Rendering::Resources::Material> material_;
        glm::vec2 position_;
        CE::Assets::Grids::CellIndex cell_;
        float speed_;

    public:
        Character(
            std::shared_ptr<const CE::Assets::Sprites::Asset> sprite,
            std::shared_ptr<const CE::Rendering::Resources::Material> material,
            glm::vec2 position,
            CE::Assets::Grids::CellIndex cell = 0,
            float speed = 160
        )
        : sprite_(std::move(sprite)), material_(std::move(material)), position_(position), cell_(cell), speed_(speed) {
            if (!sprite_ || !material_ || cell_ >= sprite_->definition().grid.cell_count() || !std::isfinite(position.x) ||
                !std::isfinite(position.y) || !std::isfinite(speed_) || speed_ <= 0)
                throw std::invalid_argument("Character requires resources, a valid cell, finite position and positive speed");
        }

        void update(CharacterCommand command, double seconds) {
            if (!std::isfinite(command.horizontal) || !std::isfinite(seconds) || seconds < 0)
                throw std::invalid_argument("Character movement requires a finite command and nonnegative time");
            const double next = position_.x + std::clamp(command.horizontal, -1.0, 1.0) * speed_ * seconds;
            if (!std::isfinite(next) || std::abs(next) > std::numeric_limits<float>::max())
                throw std::invalid_argument("Character movement exceeds the position range");
            position_.x = static_cast<float>(next);
        }

        void append(CE::Rendering::PassWriter& pass) const {
            const CE::Rendering::Submission::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
            CE::Rendering::DrawStyle2D style;
            style.material = material_;
            style.scale = 4;
            style.model_matrix = glm::translate(glm::mat4(1), glm::vec3(position_, 0));
            pass.add(CE::Rendering::Submission::resolve_sprite(*sprite_, cell_, style, context));
        }

        [[nodiscard]] glm::vec2 position() const { return position_; }
        [[nodiscard]] float speed() const { return speed_; }
    };
}
