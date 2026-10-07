#include <gtest/gtest.h>
#include <assets/definitions/animation.h>
#include <internals/exceptions.h>

#include <chrono>

namespace {
    using namespace std::chrono_literals;

    CE::Assets::TileAnimationDefinition clip() {
        CE::Assets::TileAnimationDefinition result;
        result.target = 9;
        result.frames = {{7, 30ms}, {3, 50ms}, {11, 20ms}};
        return result;
    }
}

TEST(tile_animation, timing) {
    const auto animation = clip();
    EXPECT_EQ(animation.cell_at(0ms), 7);
    EXPECT_EQ(animation.cell_at(29ms), 7);
    EXPECT_EQ(animation.cell_at(30ms), 3);
    EXPECT_EQ(animation.cell_at(79ms), 3);
    EXPECT_EQ(animation.cell_at(80ms), 11);
    EXPECT_EQ(animation.cell_at(99ms), 11);
    EXPECT_EQ(animation.cell_at(100ms), 11);
    EXPECT_EQ(animation.cell_at(std::chrono::milliseconds::max()), 11);
}

TEST(tile_animation, loop) {
    auto animation = clip();
    animation.loop = true;
    EXPECT_EQ(animation.cell_at(100ms), 7);
    EXPECT_EQ(animation.cell_at(129ms), 7);
    EXPECT_EQ(animation.cell_at(130ms), 3);
    EXPECT_EQ(animation.cell_at(180ms), 11);
    EXPECT_EQ(animation.cell_at(200ms), 7);
    EXPECT_EQ(animation.cell_at(80ms), 11); // Lookup order does not establish a clock or cursor.
    const auto maximum = std::chrono::milliseconds::max();
    EXPECT_EQ(animation.cell_at(maximum), animation.cell_at(maximum % 100ms));
}

TEST(tile_animation, invalid) {
    auto animation = clip();
    EXPECT_THROW(static_cast<void>(animation.cell_at(-1ms)), CE::Exceptions::invalid_args);
    EXPECT_EQ(animation.cell_at(30ms), 3);
    animation.frames.back().duration = 0ms;
    EXPECT_THROW(static_cast<void>(animation.cell_at(0ms)), CE::Exceptions::invalid_args);
    animation.frames.back().duration = -1ms;
    EXPECT_THROW(static_cast<void>(animation.cell_at(0ms)), CE::Exceptions::invalid_args);
    animation.frames.clear();
    EXPECT_THROW(static_cast<void>(animation.cell_at(0ms)), CE::Exceptions::invalid_args);
}

TEST(tile_animation, duration_limit) {
    auto animation = clip();
    const auto maximum = std::chrono::milliseconds::max();
    animation.frames = {{7, maximum}};
    EXPECT_EQ(animation.cell_at(maximum - 1ms), 7);
    EXPECT_EQ(animation.cell_at(maximum), 7);
    animation.loop = true;
    EXPECT_EQ(animation.cell_at(maximum), 7);
    animation.frames.push_back({3, 1ms});
    EXPECT_THROW(static_cast<void>(animation.cell_at(0ms)), CE::Exceptions::invalid_args);
}
