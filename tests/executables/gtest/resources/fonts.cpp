#include <gtest/gtest.h>

#include <core/resources/fileio/fonts-system.h>
#include <assets/resources/resource-provider.h>
#include <assets/types/2d/font-upload-internal.h>
#include <internals/exceptions.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
    namespace fs = std::filesystem;

    /** Owns a disposable font tree so discovery uses only test-created files. */
    struct TemporaryDirectory {
        fs::path path;

        TemporaryDirectory() {
            const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
            path = fs::temp_directory_path() / ("cheryl-font-test-" + std::to_string(suffix));
            fs::create_directories(path);
        }

        ~TemporaryDirectory() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    };

    using namespace CE::Assets;
    constexpr std::size_t glyph_vertex_count = font_character_count * CE::VAONumbers::vertices_per_quad;

    struct UploadedGlyphs final : Geometry2D {
        std::vector<CE::Vertex2D> vertices;

        explicit UploadedGlyphs(
            const std::span<const CE::Vertex2D> vertices
        )
        : vertices(vertices.begin(), vertices.end()) {}

        [[nodiscard]] VertexLayout2D vertex_layout() const noexcept override { return VertexLayout2D::Position3UV2; }
        [[nodiscard]] PrimitiveTopology topology() const noexcept override { return PrimitiveTopology::Triangles; }
        [[nodiscard]] std::size_t vertex_count() const noexcept override { return vertices.size(); }
        void bind() const override {}
        void draw(std::size_t, std::size_t) const override {}
    };

    struct UploadedAtlas final : Image {
        PixelSize size;
        std::vector<unsigned char> pixels;

        UploadedAtlas(
            const PixelSize size,
            const std::span<const unsigned char> pixels
        )
        : size(size), pixels(pixels.begin(), pixels.end()) {}

        [[nodiscard]] PixelSize pixel_size() const override { return size; }
        void bind(std::uint32_t) const override {}
    };

    struct FontUploadProvider final : ResourceProvider {
        enum class AtlasResult { Success, Throw, Null };
        AtlasResult atlas_result = AtlasResult::Success;
        bool reject_geometry = false;
        int geometry_calls = 0;
        int atlas_calls = 0;
        std::weak_ptr<CE::Vertex2D> cpu_vertices;
        std::weak_ptr<Geometry2D> uploaded_geometry;
        std::weak_ptr<Image> uploaded_atlas;

        std::shared_ptr<Image> create_image(const DecodedImage&) override { return {}; }
        std::shared_ptr<Shader> link_program(const std::vector<fs::path>&) override { return {}; }

        std::shared_ptr<Geometry2D> upload_geometry(
            const std::span<const CE::Vertex2D> vertices,
            const PrimitiveTopology topology
        ) override {
            ++geometry_calls;
            EXPECT_FALSE(cpu_vertices.expired());
            EXPECT_EQ(vertices.size(), glyph_vertex_count);
            EXPECT_EQ(topology, PrimitiveTopology::Triangles);
            if (reject_geometry)
                throw CE::Exceptions::failed_operation(CE_HERE, "Rejected glyph geometry");
            auto geometry = std::make_shared<UploadedGlyphs>(vertices);
            uploaded_geometry = geometry;
            return geometry;
        }

        std::shared_ptr<Image> create_font_atlas(
            const std::span<const unsigned char> alpha,
            const PixelSize size
        ) override {
            ++atlas_calls;
            EXPECT_TRUE(cpu_vertices.expired());
            EXPECT_FALSE(uploaded_geometry.expired());
            if (atlas_result == AtlasResult::Throw)
                throw CE::Exceptions::failed_operation(CE_HERE, "Rejected font atlas");
            if (atlas_result == AtlasResult::Null)
                return {};
            auto atlas = std::make_shared<UploadedAtlas>(size, alpha);
            uploaded_atlas = atlas;
            return atlas;
        }
    };

    std::shared_ptr<CE::Vertex2D> glyph_vertices(
        FontUploadProvider& provider
    ) {
        auto storage = std::make_shared<std::array<CE::Vertex2D, glyph_vertex_count>>();
        storage->front().x = 17;
        std::shared_ptr<CE::Vertex2D> vertices(storage, storage->data());
        provider.cpu_vertices = vertices;
        return vertices;
    }

    void create_empty_file(const fs::path& path) {
        fs::create_directories(path.parent_path());
        std::ofstream(path).put('\0');
    }
}

TEST(system_fonts, font_discovery) {
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

TEST(system_fonts, preferred_font) {
    // A preferred face wins even when its filename uses uppercase letters.
    const std::vector<fs::path> fonts{"/fonts/Zeta.ttf", "/fonts/DejaVuSans.ttf", "/fonts/ARIAL.TTF"};
    ASSERT_TRUE(CE::Resources::select_default_system_font(fonts).has_value());
    EXPECT_EQ(*CE::Resources::select_default_system_font(fonts), fs::path("/fonts/ARIAL.TTF"));
}

TEST(system_fonts, fallback_font) {
    // Without a preferred face, select the first sorted path; an empty list has no selection.
    const std::vector<fs::path> fallback{"/fonts/ZetaCustom.otf", "/fonts/AlphaCustom.otf"};
    ASSERT_TRUE(CE::Resources::select_default_system_font(fallback).has_value());
    EXPECT_EQ(*CE::Resources::select_default_system_font(fallback), fs::path("/fonts/AlphaCustom.otf"));
    EXPECT_FALSE(CE::Resources::select_default_system_font({}).has_value());
}

TEST(font_upload, rejected_geometry_releases_cpu_storage_without_starting_the_atlas) {
    FontUploadProvider provider;
    provider.reject_geometry = true;
    auto vertices = glyph_vertices(provider);
    const std::array<unsigned char, 3> alpha{1, 2, 3};
    const std::array<float, font_character_count> advances{};
    EXPECT_THROW(
        (void)FontDetail::upload_baked_font(provider, std::move(vertices), alpha, PixelSize{3, 1}, advances, 12),
        CE::Exceptions::failed_operation
    );
    EXPECT_TRUE(provider.cpu_vertices.expired());
    EXPECT_TRUE(provider.uploaded_geometry.expired());
    EXPECT_EQ(provider.geometry_calls, 1);
    EXPECT_EQ(provider.atlas_calls, 0);
}

TEST(font_upload, atlas_throw_or_null_releases_the_completed_glyph_geometry) {
    for (const auto outcome : {FontUploadProvider::AtlasResult::Throw, FontUploadProvider::AtlasResult::Null}) {
        SCOPED_TRACE(outcome == FontUploadProvider::AtlasResult::Throw ? "atlas throws" : "atlas returns null");
        FontUploadProvider provider;
        provider.atlas_result = outcome;
        auto vertices = glyph_vertices(provider);
        const std::array<unsigned char, 3> alpha{1, 2, 3};
        const std::array<float, font_character_count> advances{};
        if (outcome == FontUploadProvider::AtlasResult::Throw) {
            EXPECT_THROW(
                (void)FontDetail::upload_baked_font(provider, std::move(vertices), alpha, PixelSize{3, 1}, advances, 12),
                CE::Exceptions::failed_operation
            );
        } else {
            EXPECT_THROW(
                (void)STBFont(FontDetail::upload_baked_font(provider, std::move(vertices), alpha, PixelSize{3, 1}, advances, 12)),
                CE::Exceptions::invalid_args
            );
        }
        EXPECT_TRUE(provider.cpu_vertices.expired());
        EXPECT_TRUE(provider.uploaded_geometry.expired());
        EXPECT_TRUE(provider.uploaded_atlas.expired());
        EXPECT_EQ(provider.geometry_calls, 1);
        EXPECT_EQ(provider.atlas_calls, 1);
    }
}

TEST(font_upload, success_retains_backend_copies_and_metrics_after_cpu_release) {
    FontUploadProvider provider;
    auto vertices = glyph_vertices(provider);
    std::array<unsigned char, 3> alpha{1, 2, 3};
    std::array<float, font_character_count> advances{};
    advances.fill(2);
    advances['A' - first_font_character] = 5;
    auto font = std::make_shared<STBFont>(
        FontDetail::upload_baked_font(provider, std::move(vertices), alpha, PixelSize{3, 1}, advances, 12)
    );
    alpha.fill(0);
    advances.fill(0);
    EXPECT_TRUE(provider.cpu_vertices.expired());
    EXPECT_FALSE(provider.uploaded_geometry.expired());
    EXPECT_FALSE(provider.uploaded_atlas.expired());
    EXPECT_EQ(font->glyph_geometry().vertex_count(), glyph_vertex_count);
    const auto* geometry = dynamic_cast<const UploadedGlyphs*>(&font->glyph_geometry());
    const auto* atlas = dynamic_cast<const UploadedAtlas*>(&font->glyph_atlas());
    ASSERT_NE(geometry, nullptr);
    ASSERT_NE(atlas, nullptr);
    EXPECT_FLOAT_EQ(geometry->vertices.front().x, 17);
    EXPECT_EQ(atlas->pixels, (std::vector<unsigned char>{1, 2, 3}));
    EXPECT_EQ(atlas->size.width, 3u);
    const auto placements = font->layout("AA\nA");
    ASSERT_EQ(placements.size(), 3u);
    EXPECT_FLOAT_EQ(placements[1].x, 5);
    EXPECT_FLOAT_EQ(placements[2].x, 0);
    EXPECT_FLOAT_EQ(placements[2].y, -12);
    font.reset();
    EXPECT_TRUE(provider.uploaded_geometry.expired());
    EXPECT_TRUE(provider.uploaded_atlas.expired());
}
