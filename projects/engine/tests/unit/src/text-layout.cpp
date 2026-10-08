#include <text/layout.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <future>
#include <limits>
#include <string>
#include <vector>

namespace {
    using namespace CE::Text;

    FontCollection fonts() {
        FontSelection selection;
        selection.automatic_system_fonts = false;
        return FontCollection::load(selection);
    }
    std::string utf8(const std::u8string_view text) {
        return {reinterpret_cast<const char*>(text.data()), text.size()};
    }
    std::vector<std::size_t> visual_sources(const ShapedText& text) {
        std::vector<std::size_t> result;
        for (const auto& glyph : text.glyphs())
            result.push_back(glyph.source.scalar_offset);
        return result;
    }
}

TEST(text_layout, languages) {
    const auto collection = fonts();
    const auto text = utf8(u8"English Français für Straße Привет мир");
    const auto shaped = layout_text(collection, text);
    ASSERT_EQ(shaped.lines().size(), 1u);
    EXPECT_GT(shaped.lines()[0].width, 0);
    EXPECT_FALSE(shaped.lines()[0].right_to_left);
    EXPECT_EQ(shaped.lines()[0].source.byte_count, text.size());
    for (const auto& glyph : shaped.glyphs()) {
        EXPECT_FALSE(glyph.missing);
        EXPECT_NE(glyph.id.glyph, 0u);
        EXPECT_LT(glyph.id.glyph, collection.faces()[glyph.id.face].glyph_count);
        EXPECT_LE(glyph.source.byte_offset + glyph.source.byte_count, text.size());
        EXPECT_LE(glyph.source.scalar_offset + glyph.source.scalar_count, shaped.scalars().size());
    }
}

TEST(text_layout, combining) {
    const auto collection = fonts();
    const auto composed = layout_text(collection, utf8(u8"é"));
    const auto decomposed = layout_text(collection, utf8(u8"e\u0301"));
    ASSERT_EQ(composed.glyphs().size(), 1u);
    ASSERT_EQ(decomposed.glyphs().size(), 1u);
    EXPECT_EQ(composed.glyphs()[0].id, decomposed.glyphs()[0].id);
    EXPECT_EQ(composed.glyphs()[0].source, (SourceRange{0, 2, 0, 1}));
    EXPECT_EQ(decomposed.glyphs()[0].source, (SourceRange{0, 3, 0, 2}));
    EXPECT_FLOAT_EQ(composed.lines()[0].width, decomposed.lines()[0].width);

    const auto ligature = layout_text(collection, "ffi");
    ASSERT_EQ(ligature.glyphs().size(), 1u);
    EXPECT_EQ(ligature.glyphs()[0].source, (SourceRange{0, 3, 0, 3}));
}

TEST(text_layout, bidi) {
    const auto collection = fonts();
    const auto rtl = layout_text(collection, utf8(u8"אבג 123"));
    ASSERT_EQ(rtl.lines().size(), 1u);
    EXPECT_TRUE(rtl.lines()[0].right_to_left);
    EXPECT_EQ(visual_sources(rtl), (std::vector<std::size_t>{4, 5, 6, 3, 2, 1, 0}));
    const auto mixed = layout_text(collection, utf8(u8"A אב B"));
    EXPECT_EQ(visual_sources(mixed), (std::vector<std::size_t>{0, 1, 3, 2, 4, 5}));
    const auto override = layout_text(collection, utf8(u8"\u202eABC\u202c"));
    EXPECT_EQ(visual_sources(override), (std::vector<std::size_t>{3, 2, 1}));
    ASSERT_EQ(override.glyphs().size(), 3u);
    EXPECT_EQ(override.glyphs()[2].source, (SourceRange{3, 1, 1, 1}));
    ASSERT_EQ(override.lines().size(), 1u);
    EXPECT_EQ(override.lines()[0].source, (SourceRange{0, 9, 0, 5}));

    LayoutOptions options;
    options.direction = ParagraphDirection::RightToLeft;
    options.maximum_width = 500;
    const auto explicit_rtl = layout_text(collection, "ABC", options);
    ASSERT_EQ(explicit_rtl.glyphs().size(), 3u);
    EXPECT_TRUE(explicit_rtl.lines()[0].right_to_left);
    EXPECT_EQ(visual_sources(explicit_rtl), (std::vector<std::size_t>{0, 1, 2})); // Latin keeps its natural run direction.
    EXPECT_FLOAT_EQ(explicit_rtl.glyphs()[0].x, 500 - explicit_rtl.lines()[0].width);
}

TEST(text_layout, bidi_controls) {
    const auto collection = fonts();
    const auto plain = layout_text(collection, "ABC");
    ASSERT_EQ(plain.glyphs().size(), 3u);
    ASSERT_EQ(plain.lines().size(), 1u);
    for (const auto sample : {u8"\u202aABC\u202c", u8"\u202bABC\u202c", u8"\u202dABC\u202c",
                             u8"\u2066ABC\u2069", u8"\u2067ABC\u2069", u8"\u2068ABC\u2069",
                             u8"\u200eABC", u8"\u200fABC", u8"\u061cABC"}) {
        const auto source = utf8(sample);
        const auto shaped = layout_text(collection, source);
        SCOPED_TRACE(static_cast<std::uint32_t>(shaped.scalars()[0].value));
        ASSERT_EQ(visual_sources(shaped), (std::vector<std::size_t>{1, 2, 3}));
        EXPECT_EQ(shaped.glyphs()[0].source, (SourceRange{shaped.scalars()[0].byte_count, 1, 1, 1}));
        for (std::size_t index = 0; index < plain.glyphs().size(); ++index) {
            EXPECT_EQ(shaped.glyphs()[index].id, plain.glyphs()[index].id);
            EXPECT_FALSE(shaped.glyphs()[index].missing);
        }
        ASSERT_EQ(shaped.lines().size(), 1u);
        EXPECT_EQ(shaped.lines()[0].source, (SourceRange{0, source.size(), 0, shaped.scalars().size()}));
        EXPECT_FLOAT_EQ(shaped.lines()[0].width, plain.lines()[0].width);
    }

    const auto nested = layout_text(collection, utf8(u8"\u202e\u202eABC\u202c\u202c"));
    EXPECT_EQ(visual_sources(nested), (std::vector<std::size_t>{4, 3, 2}));

    const auto controls = utf8(u8"\u061c\u200e\u200f\u202a\u202b\u202c\u202d\u202e\u2066\u2067\u2068\u2069");
    const auto empty = layout_text(collection, controls);
    EXPECT_TRUE(empty.glyphs().empty());
    ASSERT_EQ(empty.lines().size(), 1u);
    EXPECT_FLOAT_EQ(empty.lines()[0].width, 0);
    EXPECT_EQ(empty.lines()[0].source, (SourceRange{0, controls.size(), 0, 12}));
}

TEST(text_layout, joiner) {
    const auto collection = fonts();
    const auto ligature = layout_text(collection, "fi");
    ASSERT_EQ(ligature.glyphs().size(), 1u);
    const auto separated = layout_text(collection, utf8(u8"f\u200ci"));
    ASSERT_EQ(separated.glyphs().size(), 2u);
    EXPECT_EQ(separated.glyphs()[0].source, (SourceRange{0, 4, 0, 2}));
    EXPECT_EQ(separated.glyphs()[1].source, (SourceRange{4, 1, 2, 1}));
    const auto first = layout_text(collection, "f");
    const auto second = layout_text(collection, "i");
    ASSERT_EQ(first.glyphs().size(), 1u);
    ASSERT_EQ(second.glyphs().size(), 1u);
    EXPECT_EQ(separated.glyphs()[0].id, first.glyphs()[0].id);
    EXPECT_EQ(separated.glyphs()[1].id, second.glyphs()[0].id);
}

TEST(text_layout, fallback) {
    auto selection = FontSelection{};
    selection.automatic_system_fonts = false;
    selection.preferred = {FontFile{std::filesystem::path(CHERYL_SOURCE_DIR) / "assets/fonts/DejaVuSans.ttf"}};
    const auto collection = FontCollection::load(selection);
    const auto unsupported = layout_text(collection, utf8(u8"中\u0301"));
    ASSERT_EQ(unsupported.glyphs().size(), 1u);
    EXPECT_TRUE(unsupported.glyphs()[0].missing);
    EXPECT_EQ(unsupported.glyphs()[0].id.face, collection.faces().size() - 1);
    EXPECT_EQ(unsupported.glyphs()[0].source, (SourceRange{0, 5, 0, 2}));
    const auto replacement = layout_text(collection, utf8(u8"\ufffd"));
    ASSERT_EQ(replacement.glyphs().size(), 1u);
    EXPECT_FALSE(replacement.glyphs()[0].missing);
    const auto malformed = layout_text(collection, "\xe1\x80" "A");
    ASSERT_EQ(malformed.scalars().size(), 2u);
    EXPECT_TRUE(malformed.scalars()[0].replaced);
    EXPECT_EQ(malformed.glyphs()[0].source, (SourceRange{0, 2, 0, 1}));
    const auto nul = layout_text(collection, std::string{"A\0B", 3});
    ASSERT_EQ(nul.glyphs().size(), 3u);
    EXPECT_TRUE(nul.glyphs()[1].missing);
}

TEST(text_layout, breaks) {
    const auto collection = fonts();
    const auto empty = layout_text(collection, "");
    EXPECT_TRUE(empty.glyphs().empty());
    ASSERT_EQ(empty.lines().size(), 1u);
    EXPECT_EQ(layout_text(collection, "\n").lines().size(), 2u);
    const auto explicit_breaks = layout_text(collection, utf8(u8"A\r\nB\u2028C\u2029D"));
    ASSERT_EQ(explicit_breaks.lines().size(), 4u);
    EXPECT_FLOAT_EQ(explicit_breaks.lines()[1].baseline, -explicit_breaks.line_height());
    const auto tab = layout_text(collection, "A\tB");
    const auto spaces = layout_text(collection, "A    B");
    EXPECT_FLOAT_EQ(tab.lines()[0].width, spaces.lines()[0].width);
    EXPECT_TRUE(layout_text(collection, " \t ").glyphs().empty());
}

TEST(text_layout, wrap) {
    const auto collection = fonts();
    LayoutOptions options;
    options.maximum_width = layout_text(collection, "word").lines()[0].width + 0.01f;
    const auto words = layout_text(collection, "word word", options);
    ASSERT_EQ(words.lines().size(), 2u);
    EXPECT_EQ(words.lines()[0].source, (SourceRange{0, 5, 0, 5}));
    EXPECT_EQ(words.lines()[1].source, (SourceRange{5, 4, 5, 4}));
    for (const auto& line : words.lines()) {
        EXPECT_LE(line.width, *options.maximum_width);
        EXPECT_FALSE(line.overflow);
    }
    options.maximum_width = layout_text(collection, utf8(u8"é")).lines()[0].width + 0.01f;
    const auto accents = layout_text(collection, utf8(u8"e\u0301e\u0301e\u0301"), options);
    ASSERT_EQ(accents.lines().size(), 3u);
    for (const auto& line : accents.lines())
        EXPECT_EQ(line.source.scalar_count, 2u);
    options.maximum_width = 0.01f;
    const auto overflow = layout_text(collection, "AB", options);
    ASSERT_EQ(overflow.lines().size(), 2u);
    EXPECT_TRUE(overflow.lines()[0].overflow);
    EXPECT_TRUE(overflow.lines()[1].overflow);
}

TEST(text_layout, owned) {
    const auto collection = fonts();
    std::string source = utf8(u8"é Привет");
    const auto shaped = layout_text(collection, source);
    source.assign(source.size(), 'X');
    EXPECT_EQ(shaped.scalars()[0].value, U'\u00e9');
    auto task = std::async(std::launch::async, [&] { return layout_text(collection, utf8(u8"é Привет")); });
    const auto parallel = task.get();
    EXPECT_EQ(visual_sources(shaped), visual_sources(parallel));
    EXPECT_FLOAT_EQ(shaped.lines()[0].width, parallel.lines()[0].width);
}

TEST(text_layout, bidi_wrap) {
    const auto collection = fonts();
    LayoutOptions options;
    options.maximum_width = layout_text(collection, utf8(u8"אבג")).lines()[0].width + 0.01f;
    const auto wrapped = layout_text(collection, utf8(u8"אבג אבג"), options);
    ASSERT_EQ(wrapped.lines().size(), 2u);
    EXPECT_TRUE(wrapped.lines()[0].right_to_left);
    EXPECT_TRUE(wrapped.lines()[1].right_to_left);
    EXPECT_EQ(wrapped.lines()[0].source, (SourceRange{0, 7, 0, 4}));
    EXPECT_EQ(wrapped.lines()[1].source, (SourceRange{7, 6, 4, 3}));
    EXPECT_EQ(visual_sources(wrapped), (std::vector<std::size_t>{2, 1, 0, 6, 5, 4}));
    for (const auto& line : wrapped.lines()) {
        EXPECT_LE(line.width, *options.maximum_width);
        EXPECT_FALSE(line.overflow);
    }
}

TEST(text_layout, invalid) {
    const auto collection = fonts();
    LayoutOptions options;
    options.pixel_height = 0;
    EXPECT_THROW(static_cast<void>(layout_text(collection, "A", options)), CE::Exceptions::invalid_args);
    options.pixel_height = 32;
    for (const auto width : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        options.maximum_width = width;
        EXPECT_THROW(static_cast<void>(layout_text(collection, "A", options)), CE::Exceptions::invalid_args);
    }
    options.maximum_width.reset();
    options.direction = static_cast<ParagraphDirection>(99);
    EXPECT_THROW(static_cast<void>(layout_text(collection, "", options)), CE::Exceptions::invalid_args);
    options.direction = ParagraphDirection::Automatic;
    options.language = "fr invalid";
    EXPECT_THROW(static_cast<void>(layout_text(collection, "A", options)), CE::Exceptions::invalid_args);
    options.language = "fr";
    EXPECT_FALSE(layout_text(collection, utf8(u8"é"), options).glyphs()[0].missing);
}
