#pragma once

#include "character.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Tutorial {
    class PatrolController final {
        float left_;
        float right_;
        bool moving_right_ = true;

    public:
        PatrolController(float left, float right)
        : left_(left), right_(right) {
            if (!std::isfinite(left) || !std::isfinite(right) || left >= right)
                throw std::invalid_argument("Patrol requires a finite interval");
        }

        [[nodiscard]] CharacterCommand read(const Character& character, double seconds) {
            if (!std::isfinite(seconds) || seconds < 0)
                throw std::invalid_argument("Patrol requires finite nonnegative time");
            if (seconds == 0)
                return {};
            const float x = character.position().x;
            if (x >= right_)
                moving_right_ = false;
            else if (x <= left_)
                moving_right_ = true;
            const double distance = moving_right_ ? static_cast<double>(right_) - x : static_cast<double>(x) - left_;
            const auto step = static_cast<double>(character.speed()) * seconds;
            const double amount = std::min(1.0, distance / step);
            return {moving_right_ ? amount : -amount};
        }
    };
}
