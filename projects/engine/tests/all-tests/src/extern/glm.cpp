#include <gtest/gtest.h>
#include <glm.hpp>

TEST(externlibs, glm_compare) {
    glm::mat4 original{0.0f};

    for (int column = 0; column < original.length(); ++column) {
        for (int row = 0; row < original[column].length(); ++row) {
            auto changed = original;
            changed[column][row] = 1.0f;

            EXPECT_NE(original, changed) << "Comparison ignored [" << column << "][" << row << "]";
        }
    }
}
