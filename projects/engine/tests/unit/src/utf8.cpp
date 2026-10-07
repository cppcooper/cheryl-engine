#include <text/utf8.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using CE::Text::Utf8Scalar;
    using CE::Text::decode_utf8;
    using CE::Text::decode_utf8_scalar;
}

TEST(utf8, ascii) {
    EXPECT_TRUE(decode_utf8("").empty());
    const std::string text{"A\0\n\t\r\x7f", 6};
    const auto scalars = decode_utf8(text);
    ASSERT_EQ(scalars.size(), text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        EXPECT_EQ(scalars[index], (Utf8Scalar{static_cast<char32_t>(text[index]), index, 1, false}));
    EXPECT_FALSE(decode_utf8_scalar(text, text.size()));
    EXPECT_FALSE(decode_utf8_scalar(text, std::numeric_limits<std::size_t>::max()));
}

TEST(utf8, byte_ranges) {
    const auto scalars = decode_utf8("A\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80");
    const std::vector<Utf8Scalar> expected{{U'A', 0, 1, false}, {U'\u00e9', 1, 2, false},
        {U'\u4e2d', 3, 3, false}, {U'\U0001f600', 6, 4, false}};
    EXPECT_EQ(scalars, expected);
}

TEST(utf8, limits) {
    const auto scalars = decode_utf8("\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80"
        "\xef\xbf\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf");
    const std::u32string expected{0x7f, 0x80, 0x7ff, 0x800, 0xd7ff, 0xe000, 0xffff, 0x10000, 0x10ffff};
    ASSERT_EQ(scalars.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(scalars[index].value, expected[index]);
        EXPECT_FALSE(scalars[index].replaced);
    }
}

TEST(utf8, replacement) {
    const auto literal = decode_utf8("\xef\xbf\xbd");
    EXPECT_EQ(literal, (std::vector<Utf8Scalar>{{U'\ufffd', 0, 3, false}}));
    EXPECT_EQ(decode_utf8("\xff"), (std::vector<Utf8Scalar>{{U'\ufffd', 0, 1, true}}));
    EXPECT_EQ(decode_utf8("\xc2" "AB"), (std::vector<Utf8Scalar>{{U'\ufffd', 0, 1, true}, {U'A', 1, 1, false}, {U'B', 2, 1, false}}));
    EXPECT_EQ(decode_utf8("\xe1\x80" "A"), (std::vector<Utf8Scalar>{{U'\ufffd', 0, 2, true}, {U'A', 2, 1, false}}));
    EXPECT_EQ(decode_utf8("\xf0\x91\x92" "A"), (std::vector<Utf8Scalar>{{U'\ufffd', 0, 3, true}, {U'A', 3, 1, false}}));
}

TEST(utf8, truncated) {
    for (const std::string_view prefix : {"\xc2", "\xe1", "\xe1\x80", "\xf1", "\xf1\x80", "\xf1\x80\x80"}) {
        EXPECT_EQ(decode_utf8(prefix), (std::vector<Utf8Scalar>{{U'\ufffd', 0, prefix.size(), true}}));
    }
}

TEST(utf8, forbidden) {
    for (const std::string_view invalid : {"\xc0\xaf", "\xc1\xbf", "\xe0\x80\xbf", "\xed\xa0\x80", "\xed\xbf\xbf",
             "\xf0\x81\x82", "\xf4\x91\x92\x93", "\xf5\x80\x80\x80", "\x80\xbf\xff"}) {
        const auto scalars = decode_utf8(invalid);
        ASSERT_EQ(scalars.size(), invalid.size());
        for (std::size_t index = 0; index < scalars.size(); ++index)
            EXPECT_EQ(scalars[index], (Utf8Scalar{U'\ufffd', index, 1, true}));
    }
}

TEST(utf8, resume) {
    const std::string_view text = "\xe1\x80\xc3\xa9" "B";
    EXPECT_EQ(decode_utf8(text), (std::vector<Utf8Scalar>{{U'\ufffd', 0, 2, true}, {U'\u00e9', 2, 2, false}, {U'B', 4, 1, false}}));
    EXPECT_EQ(decode_utf8_scalar(text, 2), (Utf8Scalar{U'\u00e9', 2, 2, false}));
    EXPECT_EQ(decode_utf8_scalar(text, 3), (Utf8Scalar{U'\ufffd', 3, 1, true})); // A continuation byte is not a scalar boundary.
}

TEST(utf8, owned) {
    std::vector<Utf8Scalar> scalars;
    {
        std::string source = "\xef\xbb\xbf" "e\xcc\x81\xef\xb7\x90";
        scalars = decode_utf8(source);
        source.assign(source.size(), 'X');
    }
    const std::vector<Utf8Scalar> expected{{U'\ufeff', 0, 3, false}, {U'e', 3, 1, false},
        {U'\u0301', 4, 2, false}, {U'\ufdd0', 6, 3, false}};
    EXPECT_EQ(scalars, expected); // No BOM stripping, normalization or combining-mark clustering.
}
