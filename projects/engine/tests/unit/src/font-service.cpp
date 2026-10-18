#include <text/font-service.h>
#include <text/font-rasterizer.h>
#include <text/layout.h>
#include <internals/exceptions.h>

#include <gtest/gtest.h>
#include <testing/font-fixture.h>

#include <algorithm>
#include <filesystem>
#include <future>
#include <random>

namespace {
    using namespace CE::Text;
    const auto fixture = std::filesystem::path(CHERYL_SOURCE_DIR) / "assets/fonts/DejaVuSans.ttf";

    using Directory = CE::Testing::FontFixture;
    using CE::Testing::collection_file;
    using CE::Testing::save;
    using CE::Testing::styled_file;

    FontConfiguration configuration() {
        FontConfiguration config;
        config.families = {{"body", {FontFile{fixture}}}, {"handwriting", {FontFile{fixture}}}, {"serif", {FontFile{fixture}}}};
        config.groups = {{"books", {"handwriting", "serif"}}};
        config.default_font = "body";
        config.asset_directories.clear();
        config.system_directories = std::vector<std::filesystem::path>{};
        return config;
    }
}

TEST(font_service, embedded) {
    FontService fonts;
    const auto font = fonts.default_font();
    EXPECT_EQ(font.reference(), "cheryl:embedded");
    ASSERT_EQ(font.faces().size(), 1u);
    EXPECT_FALSE(font.faces()[0].fallback);
    EXPECT_FALSE(font.faces()[0].bytes.empty());
    EXPECT_EQ(fonts.get("missing").reference(), font.reference());
    EXPECT_EQ(fonts.choose_random("missing").reference(), font.reference());
    EXPECT_EQ(fonts.all().size(), 1u);
    EXPECT_FALSE(layout_text(font, "Hello").glyphs().empty());
    EXPECT_TRUE(FontRasterizer::load(font).covers(U'A'));
}

TEST(font_service, default_font) {
    FontService fonts;
    auto config = configuration();
    config.fallbacks = {"serif"};
    fonts.configure(config);
    const auto font = fonts.default_font();
    EXPECT_EQ(font.reference(), "body");
    ASSERT_EQ(font.faces().size(), 3u);
    EXPECT_FALSE(font.faces()[0].fallback);
    EXPECT_TRUE(font.faces()[1].fallback);
    EXPECT_TRUE(font.faces()[2].fallback);
    EXPECT_EQ(font.faces()[1].family_index, 1u);
    EXPECT_EQ(fonts.get("unknown").reference(), "body");
    EXPECT_EQ(fonts.choose_random("empty").reference(), "body");
    EXPECT_EQ(fonts.all().size(), 4u);
}

TEST(font_service, fallback_order) {
    FontService fonts;
    auto config = configuration();
    config.fallbacks = {"body", "serif", "body", "unavailable"};
    fonts.configure(config);
    const auto fallback = fonts.fallbacks();
    ASSERT_EQ(fallback.size(), 3u);
    EXPECT_EQ(fallback[0].reference(), "body");
    EXPECT_EQ(fallback[1].reference(), "serif");
    EXPECT_EQ(fallback[2].reference(), "cheryl:embedded");
    EXPECT_EQ(fonts.default_font().faces()[1].family_index, 1u);
    EXPECT_EQ(fonts.get("handwriting").faces().size(), 4u);
}

TEST(font_service, saved_choice) {
    FontService fonts;
    auto config = configuration();
    fonts.configure(config);
    std::mt19937_64 random{42};
    const auto chosen = fonts.choose_random("books", random);
    EXPECT_TRUE(chosen.reference() == "handwriting" || chosen.reference() == "serif");
    const auto unchanged = random;
    EXPECT_EQ(fonts.restore_or_choose("books", chosen.reference(), random).reference(), chosen.reference());
    EXPECT_EQ(random, unchanged);

    std::reverse(config.families.begin(), config.families.end());
    std::reverse(config.groups["books"].begin(), config.groups["books"].end());
    fonts.configure(config);
    EXPECT_EQ(fonts.restore_or_choose("books", chosen.reference(), random).reference(), chosen.reference());
    EXPECT_EQ(random, unchanged);

    config.groups["books"] = {chosen.reference() == "handwriting" ? "serif" : "handwriting"};
    fonts.configure(config);
    const auto replacement = fonts.restore_or_choose("books", chosen.reference(), random);
    EXPECT_EQ(replacement.reference(), config.groups["books"][0]);
    EXPECT_NE(replacement.reference(), chosen.reference());
}

TEST(font_service, unavailable) {
    Directory directory;
    auto config = configuration();
    config.families[1].sources = {FontFile{directory.path / "absent.ttf"}};
    const auto corrupt = directory.path / "corrupt.ttf";
    save(corrupt, {'n', 'o', 't', '-', 'a', '-', 'f', 'o', 'n', 't'});
    config.families[2].sources = {FontFile{corrupt}};
    FontService fonts;
    fonts.configure(config);
    std::mt19937_64 random{10};
    EXPECT_EQ(fonts.restore_or_choose("books", "handwriting", random).reference(), "body");
    EXPECT_EQ(fonts.get("serif").reference(), "body");
    config.default_font = "serif";
    fonts.configure(config);
    EXPECT_EQ(fonts.choose_random("books", random).reference(), "cheryl:embedded");
}

TEST(font_service, snapshot) {
    Font retained;
    {
        Directory directory;
        const auto file = directory.path / "font.ttf";
        std::filesystem::copy_file(fixture, file);
        auto config = configuration();
        config.families = {{"body", {FontFile{file}}}};
        FontService fonts;
        fonts.configure(config);
        retained = fonts.default_font();
        EXPECT_TRUE(std::filesystem::remove(file));
        EXPECT_EQ(fonts.default_font().reference(), "body");
        EXPECT_TRUE(FontRasterizer::load(fonts.default_font()).covers(U'A'));
        fonts.configure(FontConfiguration{});
        EXPECT_EQ(fonts.default_font().reference(), "cheryl:embedded");
    }
    EXPECT_EQ(retained.reference(), "body");
    EXPECT_FALSE(layout_text(retained, "A retained book").glyphs().empty());
    EXPECT_TRUE(FontRasterizer::load(retained.faces()[0]).covers(U'\u0416'));
}

TEST(font_service, styles) {
    Directory directory;
    styled_file(directory.path / "regular.ttf", 400, false);
    styled_file(directory.path / "bold.ttf", 700, false);
    styled_file(directory.path / "italic.ttf", 400, true);
    auto config = configuration();
    config.families = {{"body", {"DejaVu Sans"}}};
    config.asset_directories = {directory.path};
    FontService fonts;
    fonts.configure(config);
    const auto font = fonts.default_font();
    ASSERT_EQ(font.faces().size(), 4u);
    EXPECT_EQ(font.collection().faces()[0].weight, 400);
    EXPECT_FALSE(font.collection().faces()[0].italic);
    EXPECT_EQ(font.collection({700, false}).faces()[0].weight, 700);
    EXPECT_TRUE(font.collection({400, true}).faces()[0].italic);
    const auto bold = FontRasterizer::load(font, {700, false});
    EXPECT_EQ(bold.properties().weight, 700);
    const auto shaped = layout_text(font, "Bold", {.font_style = {700, false}});
    EXPECT_EQ(shaped.fonts().faces()[0].weight, 700);
}

TEST(font_service, face_index) {
    Directory directory;
    const auto file = directory.path / "collection.ttc";
    collection_file(file);
    auto config = configuration();
    config.families = {{"body", {FontFile{file, 1}}}};
    FontService fonts;
    fonts.configure(config);
    const auto font = fonts.default_font();
    ASSERT_EQ(font.reference(), "body");
    ASSERT_EQ(font.faces().size(), 2u);
    EXPECT_EQ(font.faces()[0].face_index, 1u);
    EXPECT_EQ(font.collection().faces()[0].face_index, 1u);
    EXPECT_TRUE(FontRasterizer::load(font).covers(U'A'));
    EXPECT_FALSE(layout_text(font, "Collection").glyphs().empty());
}

TEST(font_service, selection) {
    FontSelection selection;
    selection.preferred = {FontFile{fixture}};
    selection.automatic_system_fonts = false;
    selection.asset_directories.clear();
    selection.system_directories = std::vector<std::filesystem::path>{};
    FontService fonts;
    fonts.configure(selection);
    const auto font = fonts.default_font();
    EXPECT_EQ(font.reference(), "application-default");
    ASSERT_EQ(font.faces().size(), 2u);
    EXPECT_EQ(font.faces()[0].family_index, 0u);
    EXPECT_TRUE(font.faces()[1].fallback);
    EXPECT_EQ(font.collection().faces()[0].path, fixture);
}

TEST(font_service, selection_references) {
    Directory directory;
    const auto first = directory.path / "first.ttf";
    const auto second = directory.path / "second.ttf";
    std::filesystem::copy_file(fixture, first);
    std::filesystem::copy_file(fixture, second);
    FontSelection selection;
    selection.preferred = {FontFile{first}, FontFile{second}};
    selection.automatic_system_fonts = false;
    selection.asset_directories.clear();
    selection.system_directories = std::vector<std::filesystem::path>{};
    FontService fonts;
    fonts.configure(selection);
    const auto fallback = fonts.fallbacks();
    ASSERT_EQ(fallback.size(), 2u);
    const auto reference = fallback.front().reference();
    EXPECT_NE(reference, "application-default");
    EXPECT_EQ(fonts.get(reference).collection().faces()[0].path, second);
    EXPECT_EQ(fallback.back().reference(), "cheryl:embedded");
    std::reverse(selection.preferred.begin(), selection.preferred.end());
    fonts.configure(selection);
    EXPECT_EQ(fonts.get(reference).reference(), reference);
    EXPECT_EQ(fonts.get(reference).collection().faces()[0].path, second);
}

TEST(font_service, named_selection) {
    FontSelection selection;
    selection.preferred = {FontFile{fixture}};
    selection.automatic_system_fonts = false;
    selection.asset_directories.clear();
    selection.system_directories = std::vector<std::filesystem::path>{};
    FontService fonts;
    fonts.configure(selection, "demo");
    EXPECT_EQ(fonts.default_font().reference(), "demo");
    EXPECT_EQ(fonts.get("demo").reference(), "demo");
    EXPECT_EQ(fonts.get("demo").collection().faces()[0].path, fixture);
    EXPECT_FALSE(layout_text(fonts.get("demo"), "Named default").glyphs().empty());
    for (const auto& reference : {std::string{}, std::string{"cheryl:embedded"}, std::string{"bad\0name", 8}}) {
        EXPECT_THROW(fonts.configure(selection, reference), CE::Exceptions::invalid_args);
        EXPECT_EQ(fonts.default_font().reference(), "demo");
    }
    const auto all = fonts.all();
    const auto source = std::ranges::find_if(all, [](const Font& font) { return font.reference().starts_with("source:"); });
    ASSERT_NE(source, all.end());
    EXPECT_THROW(fonts.configure(selection, source->reference()), CE::Exceptions::invalid_args);
    EXPECT_EQ(fonts.default_font().reference(), "demo");
}

TEST(font_service, invalid) {
    FontService fonts;
    auto config = configuration();
    fonts.configure(config);
    config.families.push_back(config.families.front());
    EXPECT_THROW(fonts.configure(config), CE::Exceptions::invalid_args);
    EXPECT_EQ(fonts.default_font().reference(), "body");
    config = configuration();
    config.families[0].sources = {""};
    EXPECT_THROW(fonts.configure(config), CE::Exceptions::invalid_args);
    config = configuration();
    config.families[0].reference = "cheryl:embedded";
    EXPECT_THROW(fonts.configure(config), CE::Exceptions::invalid_args);
    Font empty;
    EXPECT_THROW(static_cast<void>(empty.reference()), CE::Exceptions::failed_operation);
    EXPECT_THROW(static_cast<void>(empty.collection()), CE::Exceptions::failed_operation);
}

TEST(font_service, concurrent) {
    FontService fonts;
    fonts.configure(configuration());
    const auto retained = fonts.default_font();
    auto worker = std::async(std::launch::async, [&fonts, retained] {
        std::mt19937_64 random{1};
        for (int i = 0; i < 8; ++i) {
            if (!fonts.restore_or_choose("books", "handwriting", random) || layout_text(retained, "Read").glyphs().empty())
                return false;
        }
        return true;
    });
    fonts.configure(FontConfiguration{});
    EXPECT_TRUE(worker.get());
}
