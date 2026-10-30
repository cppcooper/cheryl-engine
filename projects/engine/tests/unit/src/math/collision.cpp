#include <cemath.h>
#include <gtest/gtest.h>

TEST(math_collision, overlap) {
    constexpr glm::vec2 position{0.0f, 0.0f};
    constexpr glm::vec2 dimensions{4.0f, 4.0f};
    static_assert(CE::Math::overlaps(position, position, dimensions, dimensions));
    static_assert(noexcept(CE::Math::overlaps(position, position, dimensions, dimensions)));

    for (const auto other : {glm::vec2{0, 0}, glm::vec2{1, 1}, glm::vec2{-2, -2}}) {
        EXPECT_TRUE(CE::Math::overlaps(position, other, dimensions, dimensions));
        EXPECT_TRUE(CE::Math::overlaps(other, position, dimensions, dimensions));
    }
    EXPECT_TRUE(CE::Math::overlaps(position, position, dimensions, {1, 2}));
    EXPECT_TRUE(CE::Math::overlaps(position, position, {1, 2}, dimensions));
}

TEST(math_collision, separation) {
    for (const auto other : {glm::vec2{5, 0}, glm::vec2{-5, 0}, glm::vec2{0, 5}, glm::vec2{0, -5}, glm::vec2{5, 5}}) {
        EXPECT_FALSE(CE::Math::overlaps({0, 0}, other, {4, 4}, {4, 4}));
        EXPECT_FALSE(CE::Math::overlaps(other, {0, 0}, {4, 4}, {4, 4}));
    }
}

TEST(math_collision, touching) {
    for (const auto other : {glm::vec2{4, 0}, glm::vec2{-4, 0}, glm::vec2{0, 4}, glm::vec2{0, -4}, glm::vec2{4, 4}}) {
        EXPECT_FALSE(CE::Math::overlaps({0, 0}, other, {4, 4}, {4, 4}));
        EXPECT_FALSE(CE::Math::overlaps(other, {0, 0}, {4, 4}, {4, 4}));
    }
}

TEST(math_collision, empty) {
    for (const auto dimensions : {glm::vec2{0, 2}, glm::vec2{2, 0}, glm::vec2{-1, 2}, glm::vec2{2, -1}}) {
        EXPECT_FALSE(CE::Math::overlaps({0, 0}, {0, 0}, dimensions, {4, 4}));
        EXPECT_FALSE(CE::Math::overlaps({0, 0}, {0, 0}, {4, 4}, dimensions));
    }
}

TEST(math_collision, anchors) {
    constexpr CE::Rendering::Geometry::Pivot center{0.5f, 0.5f};
    constexpr CE::Rendering::Geometry::Pivot top_left{0, 0};
    constexpr CE::Rendering::Geometry::Pivot bottom_right{1, 1};

    EXPECT_FALSE(CE::Math::overlaps({0, 0}, {3, -1}, {4, 2}, {2, 2}));
    EXPECT_TRUE(CE::Math::overlaps({0, 0}, {3, -1}, {4, 2}, {2, 2}, top_left));
    EXPECT_TRUE(CE::Math::overlaps({3, -1}, {0, 0}, {2, 2}, {4, 2}, center, top_left));
    EXPECT_FALSE(CE::Math::overlaps({0, 0}, {1, 1}, {4, 2}, {2, 2}, top_left));

    EXPECT_FALSE(CE::Math::overlaps({0, 0}, {-3, 1}, {4, 2}, {2, 2}));
    EXPECT_TRUE(CE::Math::overlaps({0, 0}, {-3, 1}, {4, 2}, {2, 2}, bottom_right));
    EXPECT_TRUE(CE::Math::overlaps({-3, 1}, {0, 0}, {2, 2}, {4, 2}, center, bottom_right));

    EXPECT_FALSE(CE::Math::overlaps({0, 0}, {3, 3}, {4, 4}, {2, 2}));
    EXPECT_TRUE(CE::Math::overlaps({0, 0}, {3, 3}, {4, 4}, {2, 2}, {0.25f, 0.75f}));
}
