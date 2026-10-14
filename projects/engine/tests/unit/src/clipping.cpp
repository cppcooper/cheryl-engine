#include <core/rendering/clip-region.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <limits>

namespace {
    using CE::Exceptions::invalid_args;
}

TEST(clipping, scaled_edges) {
    const CE::Rendering::ClipRegion2D clip{{10.25, 20.5, 60.25, 80.75}, 100, 100};
    EXPECT_EQ(CE::Rendering::resolve_clip_region(clip, {200, 300}), (CE::Rendering::PixelClipRect2D{20, 61, 121, 243}));
    EXPECT_EQ(CE::Rendering::resolve_clip_region(clip, {100, 100}), (CE::Rendering::PixelClipRect2D{10, 20, 61, 81}));
}

TEST(clipping, viewport_bounds) {
    EXPECT_EQ(
        CE::Rendering::resolve_clip_region({{-50, -20, 180, 120}, 100, 100}, {200, 300}), (CE::Rendering::PixelClipRect2D{0, 0, 200, 300})
    );
    EXPECT_TRUE(CE::Rendering::resolve_clip_region({{110, 0, 120, 10}, 100, 100}, {200, 300}).empty());
    EXPECT_TRUE(CE::Rendering::resolve_clip_region({{-20, -20, -10, -10}, 100, 100}, {200, 300}).empty());
}

TEST(clipping, empty_regions) {
    EXPECT_TRUE(CE::Rendering::resolve_clip_region({{0.25, 0, 0.25, 10}, 100, 100}, {200, 300}).empty());
    EXPECT_TRUE(CE::Rendering::resolve_clip_region({{0, 5.5, 10, 5.5}, 100, 100}, {200, 300}).empty());
    EXPECT_TRUE(CE::Rendering::resolve_clip_region({{0, 0, 10, 10}, 100, 100}, {0, 300}).empty());
    EXPECT_TRUE(CE::Rendering::resolve_clip_region({{0, 0, 10, 10}, 100, 100}, {200, 0}).empty());
}

TEST(clipping, nested_regions) {
    EXPECT_EQ(CE::Rendering::intersect_clip_rects({0, 0, 100, 100}, {20, -10, 120, 40}), (CE::Rendering::ClipRect2D{20, 0, 100, 40}));
    const auto empty = CE::Rendering::intersect_clip_rects({0, 0, 10, 10}, {20, 30, 40, 50});
    EXPECT_TRUE(CE::Rendering::resolve_clip_region({empty, 100, 100}, {200, 200}).empty());
}

TEST(clipping, invalid_regions) {
    const auto infinity = std::numeric_limits<double>::infinity();
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    for (const auto rectangle :
         {CE::Rendering::ClipRect2D{2, 0, 1, 1}, CE::Rendering::ClipRect2D{0, 2, 1, 1}, CE::Rendering::ClipRect2D{nan, 0, 1, 1},
          CE::Rendering::ClipRect2D{0, 0, infinity, 1}}) {
        EXPECT_THROW(CE::Rendering::validate_clip_region((CE::Rendering::ClipRegion2D{rectangle, 100, 100})), invalid_args);
        EXPECT_THROW((void)CE::Rendering::intersect_clip_rects(rectangle, {}), invalid_args);
    }
    for (const auto extent : {0.0, -1.0, nan, infinity}) {
        EXPECT_THROW(CE::Rendering::validate_clip_region((CE::Rendering::ClipRegion2D{{}, extent, 100})), invalid_args);
        EXPECT_THROW(CE::Rendering::validate_clip_region((CE::Rendering::ClipRegion2D{{}, 100, extent})), invalid_args);
    }
    EXPECT_THROW((void)CE::Rendering::resolve_clip_region({{}, 100, 100}, {-1, 10}), invalid_args);
}

TEST(clipping, extreme_regions) {
    const auto limit = std::numeric_limits<double>::max();
    const auto pixels = std::numeric_limits<int>::max();
    EXPECT_EQ(
        CE::Rendering::resolve_clip_region({{-limit, -limit, limit, limit}, 1, 1}, {pixels, pixels}),
        (CE::Rendering::PixelClipRect2D{0, 0, pixels, pixels})
    );
    const auto tiny = std::numeric_limits<double>::min();
    EXPECT_EQ(
        CE::Rendering::resolve_clip_region({{0, 0, tiny, tiny}, tiny, tiny}, {pixels, pixels}),
        (CE::Rendering::PixelClipRect2D{0, 0, pixels, pixels})
    );
}
