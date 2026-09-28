#pragma once

#include <glm.hpp>

namespace CE::detail {
    // The vendored GLM's vector operator== only compares x. Inspect every scalar so
    // camera changes confined to other rows still advance revisions and reach shaders.
    [[nodiscard]] inline bool matrix_equal(const glm::mat4& left, const glm::mat4& right) {
        for (glm::length_t column = 0; column < 4; ++column) {
            for (glm::length_t row = 0; row < 4; ++row) {
                if (left[column][row] != right[column][row])
                    return false;
            }
        }
        return true;
    }
}
