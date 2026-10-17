#pragma once

#include <cheryl/assets/submission/draw2d.h>
#include <cheryl/core/rendering/render-frame.h>

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

namespace Tutorial {
    class Enemy final {
        std::shared_ptr<const CE::Assets::Sprites::Asset> sprite_;
        std::shared_ptr<const CE::Rendering::Resources::Material> material_;
        glm::vec2 position_;
        float left_;
        float right_;
        double phase_ = 0;
        double speed_ = 80;

    public:
        Enemy(
            std::shared_ptr<const CE::Assets::Sprites::Asset> sprite,
            std::shared_ptr<const CE::Rendering::Resources::Material> material,
            float left,
            float right,
            float y
        )
        : sprite_(std::move(sprite)), material_(std::move(material)), position_(left, y), left_(left), right_(right) {
            if (!sprite_ || !material_ || !std::isfinite(left) || !std::isfinite(right) || !std::isfinite(y) || left >= right)
                throw std::invalid_argument("Enemy requires resources and a finite patrol interval");
        }

        void update(double seconds) {
            if (!std::isfinite(seconds) || seconds < 0)
                throw std::invalid_argument("Enemy time must be finite and nonnegative");
            const double extent = static_cast<double>(right_) - left_;
            const double period = 2 * extent;
            phase_ = std::fmod(phase_ + std::fmod(seconds, period / speed_) * speed_, period);
            const double offset = phase_ <= extent ? phase_ : period - phase_;
            position_.x = static_cast<float>(static_cast<double>(left_) + offset);
        }

        void append(CE::Rendering::PassWriter& pass) const {
            const CE::Rendering::Submission::Context2D context{
                pass.semantics(), pass.parameters(), pass.constraints(), CE::Rendering::Submission::ImageParameter2D{"image", 0}
            };
            CE::Rendering::DrawStyle2D style;
            style.material = material_;
            style.scale = 4.0f;
            style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(position_, 0.0f));
            pass.add(CE::Rendering::Submission::resolve_sprite(*sprite_, 1, style, context));
        }
    };
}
