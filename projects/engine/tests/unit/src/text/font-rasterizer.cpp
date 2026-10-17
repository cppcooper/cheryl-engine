#include <text/font-rasterizer.h>
#include <assets/resources/decoded-image.h>
#include <internals/exceptions.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>

namespace {
    std::vector<unsigned char> font_bytes() {
        std::ifstream file(std::filesystem::path(CHERYL_SOURCE_DIR) / "assets/fonts/DejaVuSans.ttf", std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
}

TEST(font_raster, snapshot) {
    auto bytes = font_bytes();
    ASSERT_FALSE(bytes.empty());
    auto face = CE::Text::FontRasterizer::load(bytes);
    std::fill(bytes.begin(), bytes.end(), 0);
    EXPECT_EQ(face.properties().family, "DejaVu Sans");
    EXPECT_TRUE(face.covers(U'A'));
    EXPECT_FALSE(face.covers(0xd800));
    EXPECT_FALSE(face.covers(0x110000));
    auto retained = face;
    face = {};
    const auto glyph = retained.rasterize(U'A', {24});
    ASSERT_TRUE(glyph);
    EXPECT_GT(glyph->width, 0u);
    EXPECT_GT(glyph->advance, 0);
    EXPECT_EQ(glyph->pixels.size(), static_cast<std::size_t>(glyph->width) * glyph->height);
    retained = {};
    EXPECT_TRUE(std::ranges::any_of(glyph->pixels, [](const unsigned char value) { return value != 0; }));
}

TEST(font_raster, metrics) {
    const auto face = CE::Text::FontRasterizer::load(font_bytes());
    const auto small = face.metrics({18});
    const auto large = face.metrics({36});
    ASSERT_TRUE(small);
    ASSERT_TRUE(large);
    EXPECT_GT(small->ascent, 0);
    EXPECT_LT(small->descent, 0);
    EXPECT_GT(large->line_spacing, small->line_spacing);
    EXPECT_GT(small->x_height, 0);
    EXPECT_EQ(small->bitmap_scale, 1);
    EXPECT_TRUE(small->has_ellipsis);
    const auto plain = face.rasterize(U'A', {24}, {.force_autohint = true});
    const auto bold = face.rasterize(U'A', {24}, {.force_autohint = true, .bold = true});
    const auto outline = face.rasterize(U'A', {24}, {.force_autohint = true, .outline = 2});
    ASSERT_TRUE(plain);
    ASSERT_TRUE(bold);
    ASSERT_TRUE(outline);
    EXPECT_EQ(bold->advance, plain->advance + 1);
    EXPECT_GT(outline->width, plain->width);
    EXPECT_GT(outline->height, plain->height);
    EXPECT_LE(face.kerning(U'A', U'V', {24}), 0);
    EXPECT_EQ(face.kerning(U'A', U'A', {}), 0);
    EXPECT_FALSE(face.metrics({}));
    EXPECT_FALSE(face.rasterize(0xd800, {24}));
    EXPECT_FALSE(face.rasterize(U'A', {24}, {.outline = -1}));
    const auto space = face.rasterize(U' ', {24});
    ASSERT_TRUE(space);
    EXPECT_GT(space->advance, 0);
    EXPECT_TRUE(space->pixels.empty());
}

TEST(font_raster, shared_sizes) {
    const auto face = CE::Text::FontRasterizer::load(font_bytes());
    const auto small = face.rasterize(U'W', {18});
    const auto large = face.rasterize(U'W', {42});
    ASSERT_TRUE(small);
    ASSERT_TRUE(large);
    auto worker = std::async(std::launch::async, [face, expected = *small] {
        for (int i = 0; i < 32; ++i) {
            const auto result = face.rasterize(U'W', {18});
            if (!result || result->pixels != expected.pixels || result->advance != expected.advance)
                return false;
        }
        return true;
    });
    for (int i = 0; i < 32; ++i) {
        const auto result = face.rasterize(U'W', {42});
        ASSERT_TRUE(result);
        EXPECT_EQ(result->pixels, large->pixels);
        EXPECT_EQ(result->advance, large->advance);
    }
    EXPECT_TRUE(worker.get());
}

TEST(image_memory, owned_rgba) {
    // A top red row and bottom blue row in an uncompressed 32-bit TGA.
    std::array<unsigned char, 26> data{0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 32, 0x28, 0, 0, 255, 255, 255, 0, 0, 128};
    const auto image = CE::Assets::Images::decode(std::span<const unsigned char>{data});
    data.fill(0);
    EXPECT_EQ(image.size.width, 1u);
    EXPECT_EQ(image.size.height, 2u);
    EXPECT_EQ(image.rgba, (std::vector<unsigned char>{255, 0, 0, 255, 0, 0, 255, 128}));
    EXPECT_THROW(static_cast<void>(CE::Assets::Images::decode(std::span<const unsigned char>{})), CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(CE::Assets::Images::decode(std::span<const unsigned char>{data})), CE::Exceptions::runtime_exception);
}
