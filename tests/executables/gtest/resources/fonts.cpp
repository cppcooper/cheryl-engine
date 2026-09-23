#include <gtest/gtest.h>

#include <core/resources/fileio/fonts-system.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
    namespace fs = std::filesystem;

    /** Owns a disposable font tree so discovery uses only test-created files. */
    struct TemporaryDirectory {
        TemporaryDirectory() {
            const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
            path = fs::temp_directory_path() / ("cheryl-font-test-" + std::to_string(suffix));
            fs::create_directories(path);
        }
        ~TemporaryDirectory() {
            std::error_code error;
            fs::remove_all(path, error);
        }
        fs::path path;
    };

    void create_empty_file(const fs::path& path) {
        fs::create_directories(path.parent_path());
        std::ofstream(path).put('\0');
    }
}

TEST(system_fonts, discovers_supported_files_case_insensitively) {
    // Populate a nested directory with mixed-case font extensions and a non-font file.
    const TemporaryDirectory directory;
    create_empty_file(directory.path / "regular.ttf");
    create_empty_file(directory.path / "nested" / "display.OTF");
    create_empty_file(directory.path / "collection.TtC");
    create_empty_file(directory.path / "ignored.txt");

    // Search the same root twice and check that only the three font paths appear, in order.
    const auto fonts = CE::Resources::find_system_fonts({directory.path, directory.path});
    ASSERT_EQ(fonts.size(), std::size_t{3});
    EXPECT_EQ(fonts[0], directory.path / "collection.TtC");
    EXPECT_EQ(fonts[1], directory.path / "nested" / "display.OTF");
    EXPECT_EQ(fonts[2], directory.path / "regular.ttf");
}

TEST(system_fonts, selects_preferred_face_then_deterministic_fallback) {
    // A preferred face wins even when its filename uses uppercase letters.
    const std::vector<fs::path> fonts{"/fonts/Zeta.ttf", "/fonts/DejaVuSans.ttf", "/fonts/ARIAL.TTF"};
    ASSERT_TRUE(CE::Resources::select_default_system_font(fonts).has_value());
    EXPECT_EQ(*CE::Resources::select_default_system_font(fonts), fs::path("/fonts/ARIAL.TTF"));

    // Without a preferred face, select the first sorted path; an empty list has no selection.
    const std::vector<fs::path> fallback{"/fonts/ZetaCustom.otf", "/fonts/AlphaCustom.otf"};
    ASSERT_TRUE(CE::Resources::select_default_system_font(fallback).has_value());
    EXPECT_EQ(*CE::Resources::select_default_system_font(fallback), fs::path("/fonts/AlphaCustom.otf"));
    EXPECT_FALSE(CE::Resources::select_default_system_font({}).has_value());
}
