#include <text/font-collection.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    using namespace CE::Text;
    const auto fixture = std::filesystem::path(CHERYL_SOURCE_DIR) / "assets/fonts/DejaVuSans.ttf";

    CE::Text::FontSelection isolated_selection() {
        CE::Text::FontSelection selection;
        selection.automatic_system_fonts = false;
        selection.asset_directories.clear();
        selection.system_directories = std::vector<std::filesystem::path>{};
        return selection;
    }

    struct FontDirectory {
        std::filesystem::path path;
        inline static std::atomic<unsigned> next{0};

        FontDirectory()
        : path(std::filesystem::temp_directory_path() / ("cheryl-font-" +
              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(next++))) {
            if (!std::filesystem::create_directory(path))
                throw std::runtime_error("Font fixture directory already exists");
        }
        ~FontDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };
}

TEST(font_selection, fallback) {
    const auto fonts = CE::Text::FontCollection::load(isolated_selection());
    ASSERT_EQ(fonts.faces().size(), 1u);
    EXPECT_TRUE(fonts.faces()[0].builtin);
    EXPECT_FALSE(fonts.faces()[0].path);
    EXPECT_EQ(fonts.faces()[0].family, "DejaVu Sans");
    for (const auto scalar : U"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
             U"\u00c0\u00c2\u00c6\u00c7\u00c8\u00c9\u00ca\u00cb\u00ce\u00cf\u00d4\u00d9\u00db\u00dc\u0152\u0178"
             U"\u00e0\u00e2\u00e6\u00e7\u00e8\u00e9\u00ea\u00eb\u00ee\u00ef\u00f4\u00f9\u00fb\u00fc\u0153\u00ff"
             U"\u00c4\u00d6\u00e4\u00f6\u00df\u1e9e\u0401\u0451\u0301\ufffd\u05d0") {
        if (scalar)
            EXPECT_TRUE(fonts.covers(scalar));
    }
    for (char32_t scalar = 0x0410; scalar <= 0x044f; ++scalar)
        EXPECT_TRUE(fonts.covers(scalar));
    EXPECT_FALSE(fonts.covers(U'\u4e2d'));
    EXPECT_FALSE(fonts.covers(static_cast<char32_t>(0xd800)));
    EXPECT_FALSE(fonts.covers(static_cast<char32_t>(0x110000)));
}

TEST(font_selection, ordered) {
    auto selection = isolated_selection();
    selection.preferred = {
        CE::Text::SystemFontFamily{"Absent Cheryl Test Family"}, CE::Text::FontFile{fixture}, CE::Text::FontFile{fixture}
    };
    const auto fonts = CE::Text::FontCollection::load(selection);
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
    selection.preferred = {CE::Text::SystemFontFamily{"dEjAvU sAnS"}};
    selection.system_directories = std::vector{directory.path};
    const auto fonts = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(fonts.faces().size(), 2u);
    EXPECT_EQ(fonts.faces()[0].path, renamed);
    EXPECT_EQ(fonts.faces()[0].family, "DejaVu Sans");

    selection.preferred.clear();
    selection.automatic_system_fonts = true;
    const auto automatic = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(automatic.faces().size(), 2u);
    EXPECT_EQ(automatic.faces()[0].path, renamed);
}

TEST(font_selection, names) {
    FontDirectory assets;
    FontDirectory system;
    const auto nested = assets.path / "nested";
    ASSERT_TRUE(std::filesystem::create_directory(nested));
    const auto application = nested / "GameFont.TTF";
    const auto installed = system.path / "installed.ttf";
    std::filesystem::copy_file(fixture, application);
    std::filesystem::copy_file(fixture, installed);
    auto selection = isolated_selection();
    selection.asset_directories = {assets.path};
    selection.system_directories = std::vector{system.path};
    selection.preferred = {"Absent Cheryl Test Family", "dEjAvU sAnS", "gamefont.ttf", "Absent.ttf"};
    const auto fonts = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(fonts.faces().size(), 2u);
    EXPECT_EQ(fonts.faces()[0].path, application);
    EXPECT_EQ(fonts.faces()[0].family, "DejaVu Sans");
    EXPECT_TRUE(fonts.faces()[1].builtin);

    selection.preferred = {"GameFont"};
    const auto no_stem = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(no_stem.faces().size(), 1u);
    EXPECT_TRUE(no_stem.faces()[0].builtin);

    selection.preferred = {CE::Text::SystemFontFamily{"dEjAvU sAnS"}};
    const auto system_only = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(system_only.faces().size(), 2u);
    EXPECT_EQ(system_only.faces()[0].path, installed);
}

TEST(font_selection, name_order) {
    FontDirectory assets;
    const auto first = assets.path / "First.ttf";
    const auto second = assets.path / "Second.ttf";
    std::filesystem::copy_file(fixture, first);
    std::filesystem::copy_file(fixture, second);
    auto selection = isolated_selection();
    selection.asset_directories = {assets.path};
    selection.preferred = {"Second.ttf", "Absent.ttf", "FIRST.TTF", "second.ttf"};
    const auto fonts = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(fonts.faces().size(), 3u);
    EXPECT_EQ(fonts.faces()[0].path, second);
    EXPECT_EQ(fonts.faces()[1].path, first);
    EXPECT_TRUE(fonts.faces()[2].builtin);

    selection.preferred = {"DejaVu Sans"};
    const auto family = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(family.faces().size(), 2u);
    EXPECT_EQ(family.faces()[0].path, first);
}

TEST(font_selection, named_system) {
    FontDirectory directory;
    const auto installed = directory.path / "installed.ttf";
    std::filesystem::copy_file(fixture, installed);
    auto selection = isolated_selection();
    selection.system_directories = std::vector{directory.path};
    selection.preferred = {"dEjAvU sAnS"};
    const auto fonts = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(fonts.faces().size(), 2u);
    EXPECT_EQ(fonts.faces()[0].path, installed);
    EXPECT_TRUE(fonts.faces()[1].builtin);

    selection.system_directories = std::vector<std::filesystem::path>{};
    const auto missing = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(missing.faces().size(), 1u);
    EXPECT_TRUE(missing.faces()[0].builtin);
}

TEST(font_selection, automatic_assets) {
    FontDirectory assets;
    FontDirectory system;
    const auto application = assets.path / "application.ttf";
    const auto installed = system.path / "installed.ttf";
    std::filesystem::copy_file(fixture, application);
    std::filesystem::copy_file(fixture, installed);
    auto selection = isolated_selection();
    selection.asset_directories = {assets.path};
    selection.system_directories = std::vector{system.path};
    selection.automatic_system_fonts = true;
    const auto fonts = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(fonts.faces().size(), 2u);
    EXPECT_EQ(fonts.faces()[0].path, application);
    EXPECT_TRUE(fonts.faces()[1].builtin);
}

TEST(font_selection, snapshot) {
    FontDirectory directory;
    const auto file = directory.path / "font.ttf";
    std::filesystem::copy_file(fixture, file);
    auto selection = isolated_selection();
    selection.preferred = {CE::Text::FontFile{file}};
    const auto fonts = CE::Text::FontCollection::load(selection);
    EXPECT_TRUE(std::filesystem::remove(file));
    EXPECT_FALSE(std::filesystem::exists(file));
    EXPECT_TRUE(fonts.covers(U'\u0416'));
    EXPECT_TRUE(fonts.covers(U'\u00e9'));
}

TEST(font_selection, missing) {
    FontDirectory directory;
    const auto first = directory.path / "first.ttf";
    const auto second = directory.path / "second.ttf";
    std::filesystem::copy_file(fixture, first);
    std::filesystem::copy_file(fixture, second);
    auto selection = isolated_selection();
    selection.preferred = {
        CE::Text::FontFile{directory.path / "absent-before.ttf"}, CE::Text::FontFile{first},
        CE::Text::FontFile{directory.path / "absent-between.ttf"}, CE::Text::FontFile{second},
        CE::Text::FontFile{directory.path / "absent-after.ttf"}
    };
    const auto fonts = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(fonts.faces().size(), 3u);
    EXPECT_EQ(fonts.faces()[0].path, first);
    EXPECT_EQ(fonts.faces()[1].path, second);
    EXPECT_TRUE(fonts.faces()[2].builtin);

    selection.preferred = {
        CE::Text::FontFile{directory.path / "absent-before.ttf"}, CE::Text::FontFile{directory.path / "absent-after.ttf"}
    };
    const auto fallback = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(fallback.faces().size(), 1u);
    EXPECT_TRUE(fallback.faces()[0].builtin);
}

TEST(font_selection, invalid) {
    auto selection = isolated_selection();
    selection.preferred = {CE::Text::FontFile{fixture, 9999}};
    EXPECT_THROW(static_cast<void>(CE::Text::FontCollection::load(selection)), CE::Exceptions::runtime_exception);
    selection.preferred = {CE::Text::SystemFontFamily{""}};
    EXPECT_THROW(static_cast<void>(CE::Text::FontCollection::load(selection)), CE::Exceptions::invalid_args);
    selection.preferred = {CE::Text::SystemFontFamily{std::string{"DejaVu\0Sans", 11}}};
    EXPECT_THROW(static_cast<void>(CE::Text::FontCollection::load(selection)), CE::Exceptions::invalid_args);
    selection.preferred = {""};
    EXPECT_THROW(static_cast<void>(CE::Text::FontCollection::load(selection)), CE::Exceptions::invalid_args);
    selection.preferred = {std::string{"DejaVu\0Sans", 11}};
    EXPECT_THROW(static_cast<void>(CE::Text::FontCollection::load(selection)), CE::Exceptions::invalid_args);
}

TEST(font_selection, corrupt) {
    FontDirectory directory;
    const auto corrupt = directory.path / "corrupt.ttf";
    {
        std::ofstream output(corrupt, std::ios::binary);
        output << "Not a font";
        ASSERT_TRUE(output.good());
    }
    const auto empty = directory.path / "empty.ttf";
    {
        std::ofstream output(empty, std::ios::binary);
        ASSERT_TRUE(output.good());
    }
    auto selection = isolated_selection();
    for (const auto& path : {corrupt, empty, directory.path}) {
        selection.preferred = {CE::Text::FontFile{path}};
        EXPECT_THROW(static_cast<void>(CE::Text::FontCollection::load(selection)), CE::Exceptions::runtime_exception);
    }

    FontDirectory system;
    const auto installed = system.path / "installed.ttf";
    std::filesystem::copy_file(fixture, installed);
    selection.asset_directories = {directory.path};
    selection.system_directories = std::vector{system.path};
    selection.preferred = {"corrupt.ttf", "empty.ttf", "DejaVu Sans"};
    const auto discovered = CE::Text::FontCollection::load(selection);
    ASSERT_EQ(discovered.faces().size(), 2u);
    EXPECT_EQ(discovered.faces()[0].path, installed);
    EXPECT_TRUE(discovered.faces()[1].builtin);
}
