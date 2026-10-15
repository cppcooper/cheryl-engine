#include <assets/types/2d/stbfont.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

    struct GlyphGeometry final : CE::Rendering::Geometry::Buffer2D {
        CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override {
            return CE::Rendering::Geometry::Layout2D::Position3UV2;
        }
        CE::Rendering::Geometry::Topology topology() const noexcept override { return CE::Rendering::Geometry::Topology::Triangles; }
        std::size_t vertex_count() const noexcept override { return CE::Text::Legacy::ascii_character_count * 6; }
        void bind() const override { throw std::logic_error("CPU font layout must not bind geometry"); }
        void draw(std::size_t, std::size_t) const override { throw std::logic_error("CPU font layout must not draw"); }
    };

    struct GlyphImage final : CE::Rendering::Resources::Image {
        CE::Rendering::Geometry::PixelSize pixel_size() const override { return {128, 128}; }
        void bind(std::uint32_t) const override { throw std::logic_error("CPU font layout must not bind an atlas"); }
    };

    CE::Text::Legacy::AsciiFontData font_data() {
        std::array<float, CE::Text::Legacy::ascii_character_count> advances;
        advances.fill(5.0f);
        advances['A' - CE::Text::Legacy::ascii_first_character] = 6.0f;
        advances['?' - CE::Text::Legacy::ascii_first_character] = 2.0f;
        advances[' ' - CE::Text::Legacy::ascii_first_character] = 3.0f;
        return {std::make_shared<GlyphGeometry>(), std::make_shared<GlyphImage>(), advances, 12.0f};
    }

    void expect_glyphs(
        const std::vector<CE::Text::Legacy::GlyphPlacement>& actual,
        const std::vector<CE::Text::Legacy::GlyphPlacement>& expected
    ) {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t index = 0; index < expected.size(); ++index) {
            EXPECT_EQ(actual[index].index, expected[index].index);
            EXPECT_FLOAT_EQ(actual[index].x, expected[index].x);
            EXPECT_FLOAT_EQ(actual[index].y, expected[index].y);
        }
    }

    constexpr std::size_t glyph(const char character) {
        return static_cast<std::size_t>(character - CE::Text::Legacy::ascii_first_character);
    }
}

TEST(stbfont, ascii) {
    const CE::Text::Legacy::AsciiFont font(font_data());
    EXPECT_TRUE(font.layout("").empty());
    EXPECT_TRUE(font.layout(" \n\t\r").empty());
    expect_glyphs(font.layout("A B\nA\r\tB\x01"), {{glyph('A'), 0.0f, 0.0f}, {glyph('B'), 9.0f, 0.0f},
        {glyph('A'), 0.0f, -12.0f}, {glyph('B'), 18.0f, -12.0f}, {glyph('?'), 23.0f, -12.0f}});
    EXPECT_THROW(static_cast<void>(font.layout("A", {.alternate_bank = true})), CE::Exceptions::invalid_args);
}

TEST(stbfont, scalar_fallback) {
    const CE::Text::Legacy::AsciiFont font(font_data());
    expect_glyphs(font.layout("A\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80" "B"), {{glyph('A'), 0.0f, 0.0f},
        {glyph('?'), 6.0f, 0.0f}, {glyph('?'), 8.0f, 0.0f}, {glyph('?'), 10.0f, 0.0f}, {glyph('B'), 12.0f, 0.0f}});
    expect_glyphs(font.layout("e\xcc\x81"), {{glyph('e'), 0.0f, 0.0f}, {glyph('?'), 5.0f, 0.0f}});
    expect_glyphs(font.layout("\xef\xbf\xbd"), {{glyph('?'), 0.0f, 0.0f}});
}

TEST(stbfont, malformed) {
    const CE::Text::Legacy::AsciiFont font(font_data());
    expect_glyphs(font.layout("\xe1\x80" "B"), {{glyph('?'), 0.0f, 0.0f}, {glyph('B'), 2.0f, 0.0f}});
    expect_glyphs(font.layout("\xc2\nA"), {{glyph('?'), 0.0f, 0.0f}, {glyph('A'), 0.0f, -12.0f}});
    expect_glyphs(font.layout("\xed\xa0\x80"), {{glyph('?'), 0.0f, 0.0f}, {glyph('?'), 2.0f, 0.0f}, {glyph('?'), 4.0f, 0.0f}});
    expect_glyphs(font.layout(std::string{"A\0B", 3}), {{glyph('A'), 0.0f, 0.0f}, {glyph('?'), 6.0f, 0.0f}, {glyph('B'), 8.0f, 0.0f}});
}

TEST(stbfont, callbacks) {
    const CE::Text::Legacy::AsciiFont font(font_data());
    const std::string text = "A\xc3\xa9\nB";
    std::vector<CE::Text::Legacy::GlyphPlacement> submitted;
    font.for_each_glyph(text, [&](const std::size_t index, const float x, const float y) { submitted.push_back({index, x, y}); });
    expect_glyphs(submitted, font.layout(text));
    std::size_t calls = 0;
    EXPECT_THROW(font.for_each_glyph(text, [&](std::size_t, float, float) {
        if (++calls == 2)
            throw std::runtime_error("submit");
    }), std::runtime_error);
    EXPECT_EQ(calls, 2u);
    expect_glyphs(font.layout(text), submitted); // A prior callback failure changes no font cursor or metrics.
}
