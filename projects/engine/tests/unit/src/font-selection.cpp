#include <text/font-collection.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace {
    using namespace CE::Text;
    const auto fixture = std::filesystem::path(CHERYL_SOURCE_DIR) / "assets/fonts/DejaVuSans.ttf";

    FontSelection isolated_selection() {
        FontSelection selection;
        selection.automatic_system_fonts = false;
        selection.system_directories = std::vector<std::filesystem::path>{};
        return selection;
    }

    struct FontDirectory {
        std::filesystem::path path;
        inline static std::atomic<unsigned> next{0};

        FontDirectory()
        : path(std::filesystem::temp_directory_path() / ("cheryl-font-" +
              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(next++))) {
            std::filesystem::create_directory(path);
        }
        ~FontDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };
}

TEST(font_selection, fallback) {
    const auto fonts = FontCollection::load(isolated_selection());
    ASSERT_EQ(fonts.faces().size(), 1u);
    EXPECT_TRUE(fonts.faces()[0].builtin);
    EXPECT_FALSE(fonts.faces()[0].path);
    EXPECT_EQ(fonts.faces()[0].family, "DejaVu Sans");
    for (const auto scalar : U"Aa\u00e9\u00fc\u00df\u0153\u0416\u044f\u0301\ufffd\u05d0") {
        if (scalar)
            EXPECT_TRUE(fonts.covers(scalar));
    }
    EXPECT_FALSE(fonts.covers(U'\u4e2d'));
    EXPECT_FALSE(fonts.covers(static_cast<char32_t>(0xd800)));
    EXPECT_FALSE(fonts.covers(static_cast<char32_t>(0x110000)));
}

TEST(font_selection, ordered) {
    auto selection = isolated_selection();
    selection.preferred = {SystemFontFamily{"Absent Cheryl Test Family"}, FontFile{fixture}, FontFile{fixture}};
    const auto fonts = FontCollection::load(selection);
    ASSERT_EQ(fonts.faces().size(), 2u); // Duplicate application source does not append another face.
    EXPECT_EQ(fonts.faces()[0].path, fixture);
    EXPECT_FALSE(fonts.faces()[0].builtin);
    EXPECT_TRUE(fonts.faces()[1].builtin);
}

TEST(font_selection, families) {
    FontDirectory directory;
    const auto renamed = directory.path / "not-a-family-name.TTF";
    std::filesystem::copy_file(fixture, renamed);
    auto selection = isolated_selection();
    selection.preferred = {SystemFontFamily{"dEjAvU sAnS"}};
    selection.system_directories = std::vector{directory.path};
    const auto fonts = FontCollection::load(selection);
    ASSERT_EQ(fonts.faces().size(), 2u);
    EXPECT_EQ(fonts.faces()[0].path, renamed);
    EXPECT_EQ(fonts.faces()[0].family, "DejaVu Sans");

    selection.preferred.clear();
    selection.automatic_system_fonts = true;
    const auto automatic = FontCollection::load(selection);
    ASSERT_EQ(automatic.faces().size(), 2u);
    EXPECT_EQ(automatic.faces()[0].path, renamed);
}

TEST(font_selection, snapshot) {
    FontDirectory directory;
    const auto file = directory.path / "font.ttf";
    std::filesystem::copy_file(fixture, file);
    auto selection = isolated_selection();
    selection.preferred = {FontFile{file}};
    const auto fonts = FontCollection::load(selection);
    std::filesystem::remove(file);
    EXPECT_FALSE(std::filesystem::exists(file));
    EXPECT_TRUE(fonts.covers(U'\u0416'));
    EXPECT_TRUE(fonts.covers(U'\u00e9'));
}

TEST(font_selection, invalid) {
    auto selection = isolated_selection();
    selection.preferred = {FontFile{fixture, 9999}};
    EXPECT_THROW(static_cast<void>(FontCollection::load(selection)), CE::Exceptions::runtime_exception);
    selection.preferred = {FontFile{fixture.parent_path() / "absent.ttf"}};
    EXPECT_THROW(static_cast<void>(FontCollection::load(selection)), CE::Exceptions::runtime_exception);
    selection.preferred = {SystemFontFamily{""}};
    EXPECT_THROW(static_cast<void>(FontCollection::load(selection)), CE::Exceptions::invalid_args);
    selection.preferred = {SystemFontFamily{std::string{"DejaVu\0Sans", 11}}};
    EXPECT_THROW(static_cast<void>(FontCollection::load(selection)), CE::Exceptions::invalid_args);
}
