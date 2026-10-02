#include <gtest/gtest.h>

#include <core/resources/fileio/fonts-system.h>
#include <assets/resources/resource-provider.h>
#include <assets/types/2d/font-upload-internal.h>
#include <assets/types/2d/font-bake-internal.h>
#include <assets/types/2d/font-stb-allocation-internal.h>
#include <testing/failing-memory-resource.h>
#include <internals/exceptions.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <memory_resource>
#include <algorithm>
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
        void draw(
            std::size_t,
            std::size_t
        ) const override {}
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
        void bind(
            std::uint32_t
        ) const override {}
    };

    struct RasterUploadProvider final : ResourceProvider {
        int geometry_calls = 0;
        int atlas_calls = 0;
        std::shared_ptr<Image> create_image(
            const DecodedImage&
        ) override {
            return {};
        }
        std::shared_ptr<Shader> link_program(
            const std::vector<fs::path>&
        ) override {
            return {};
        }
        std::shared_ptr<Geometry2D> upload_geometry(
            const std::span<const CE::Vertex2D> vertices,
            const PrimitiveTopology topology
        ) override {
            ++geometry_calls;
            EXPECT_EQ(vertices.size(), glyph_vertex_count);
            EXPECT_EQ(topology, PrimitiveTopology::Triangles);
            return std::make_shared<UploadedGlyphs>(vertices);
        }
        std::shared_ptr<Image> create_font_atlas(
            const std::span<const unsigned char> alpha,
            const PixelSize size
        ) override {
            ++atlas_calls;
            EXPECT_TRUE(std::any_of(alpha.begin(), alpha.end(), [](const auto value) { return value != 0; }));
            return std::make_shared<UploadedAtlas>(size, alpha);
        }
    };

    struct NestedBakeMemoryResource final : std::pmr::memory_resource {
        CE::Testing::FailingMemoryResource memory;
        std::function<void()> nested_bake;

    private:
        void* do_allocate(
            const std::size_t bytes,
            const std::size_t alignment
        ) override {
            // Start the nested bake while the outer glyph already owns storage.
            if (memory.requests.load() == 1)
                nested_bake();
            return memory.allocate(bytes, alignment);
        }
        void do_deallocate(
            void* pointer,
            const std::size_t bytes,
            const std::size_t alignment
        ) override {
            memory.deallocate(pointer, bytes, alignment);
        }
        bool do_is_equal(
            const std::pmr::memory_resource& other
        ) const noexcept override {
            return this == &other;
        }
    };

    void reject_each_stb_allocation(
        const fs::path& path
    ) {
        CE::Testing::FailingMemoryResource baseline;
        RasterUploadProvider successful;
        {
            const auto font = FontDetail::load_font_with_resource(path, 128, successful, baseline);
            EXPECT_NE(font.geometry, nullptr);
            EXPECT_NE(font.texture, nullptr);
            EXPECT_GT(font.line_height, 0);
            const auto geometry = std::dynamic_pointer_cast<UploadedGlyphs>(font.geometry);
            ASSERT_NE(geometry, nullptr);
            const auto first = ('W' - first_font_character) * CE::VAONumbers::vertices_per_quad;
            // This face exercises stb's heap scanline path (glyphs wider than 64 pixels).
            EXPECT_GT(geometry->vertices[first + 1].x - geometry->vertices[first].x, 64);
        }
        ASSERT_EQ(successful.geometry_calls, 1);
        ASSERT_EQ(successful.atlas_calls, 1);
        ASSERT_EQ(baseline.outstanding.load(), 0u);
        const auto requests = baseline.requests.load();
        ASSERT_GT(requests, 100u);
        ::testing::Test::RecordProperty("stb_allocation_requests", static_cast<int>(requests));
        for (std::size_t request = 1; request <= requests; ++request) {
            SCOPED_TRACE(request);
            CE::Testing::FailingMemoryResource memory;
            memory.reject_request = request;
            RasterUploadProvider rejected;
            EXPECT_THROW((void)FontDetail::load_font_with_resource(path, 128, rejected, memory), std::bad_alloc);
            EXPECT_EQ(memory.requests.load(), request);
            EXPECT_EQ(memory.rejected.load(), 1u);
            EXPECT_EQ(memory.outstanding.load(), 0u);
            EXPECT_EQ(rejected.geometry_calls, 0);
            EXPECT_EQ(rejected.atlas_calls, 0);
        }
        // A failed bake must restore the thread's allocation scope. Reuse the
        // public loader immediately after the final failure, then this resource.
        RasterUploadProvider recovered;
        EXPECT_NO_THROW((void)STBFont::load_font(path, 128, recovered));
        EXPECT_NO_THROW((void)FontDetail::load_font_with_resource(path, 128, recovered, baseline));
        EXPECT_EQ(recovered.geometry_calls, 2);
        EXPECT_EQ(recovered.atlas_calls, 2);
        EXPECT_EQ(baseline.outstanding.load(), 0u);
    }

    struct FontUploadProvider final : ResourceProvider {
        enum class AtlasResult { Success, Throw, Null };
        AtlasResult atlas_result = AtlasResult::Success;
        bool reject_geometry = false;
        int geometry_calls = 0;
        int atlas_calls = 0;
        std::weak_ptr<CE::Vertex2D> cpu_vertices;
        std::weak_ptr<Geometry2D> uploaded_geometry;
        std::weak_ptr<Image> uploaded_atlas;

        std::shared_ptr<Image> create_image(
            const DecodedImage&
        ) override {
            return {};
        }
        std::shared_ptr<Shader> link_program(
            const std::vector<fs::path>&
        ) override {
            return {};
        }

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

    void create_empty_file(
        const fs::path& path
    ) {
        fs::create_directories(path.parent_path());
        std::ofstream(path).put('\0');
    }
}

TEST(
    system_fonts,
    font_discovery
) {
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

TEST(
    system_fonts,
    preferred_font
) {
    // A preferred face wins even when its filename uses uppercase letters.
    const std::vector<fs::path> fonts{"/fonts/Zeta.ttf", "/fonts/DejaVuSans.ttf", "/fonts/ARIAL.TTF"};
    ASSERT_TRUE(CE::Resources::select_default_system_font(fonts).has_value());
    EXPECT_EQ(*CE::Resources::select_default_system_font(fonts), fs::path("/fonts/ARIAL.TTF"));
}

TEST(
    system_fonts,
    fallback_font
) {
    // Without a preferred face, select the first sorted path; an empty list has no selection.
    const std::vector<fs::path> fallback{"/fonts/ZetaCustom.otf", "/fonts/AlphaCustom.otf"};
    ASSERT_TRUE(CE::Resources::select_default_system_font(fallback).has_value());
    EXPECT_EQ(*CE::Resources::select_default_system_font(fallback), fs::path("/fonts/AlphaCustom.otf"));
    EXPECT_FALSE(CE::Resources::select_default_system_font({}).has_value());
}

TEST(
    font_bake,
    real_nested_bakes_restore_the_outer_allocation_scope
) {
    const auto* ttf = std::getenv("CHERYL_STB_ALLOCATION_TTF");
    const auto* cff = std::getenv("CHERYL_STB_ALLOCATION_CFF");
    if (!ttf || !*ttf || !cff || !*cff)
        GTEST_SKIP() << "Set CHERYL_STB_ALLOCATION_TTF and CHERYL_STB_ALLOCATION_CFF to acceptance faces";
    for (const bool reject_outer : {false, true}) {
        SCOPED_TRACE(reject_outer);
        NestedBakeMemoryResource outer;
        CE::Testing::FailingMemoryResource inner;
        inner.reject_request = 3;
        RasterUploadProvider nested;
        int nested_calls = 0;
        outer.nested_bake = [&] {
            ++nested_calls;
            EXPECT_THROW((void)FontDetail::load_font_with_resource(cff, 128, nested, inner), std::bad_alloc);
            EXPECT_EQ(inner.outstanding.load(), 0u);
            EXPECT_EQ(outer.memory.outstanding.load(), 1u);
        };
        if (reject_outer)
            outer.memory.reject_request = 3;
        RasterUploadProvider provider;
        if (reject_outer) {
            EXPECT_THROW((void)FontDetail::load_font_with_resource(ttf, 128, provider, outer), std::bad_alloc);
            EXPECT_EQ(provider.geometry_calls, 0);
            EXPECT_EQ(provider.atlas_calls, 0);
            EXPECT_EQ(outer.memory.rejected.load(), 1u);
        } else {
            EXPECT_NO_THROW((void)FontDetail::load_font_with_resource(ttf, 128, provider, outer));
            EXPECT_EQ(provider.geometry_calls, 1);
            EXPECT_EQ(provider.atlas_calls, 1);
        }
        EXPECT_EQ(nested_calls, 1);
        EXPECT_EQ(inner.rejected.load(), 1u);
        EXPECT_EQ(inner.outstanding.load(), 0u);
        EXPECT_EQ(nested.geometry_calls, 0);
        EXPECT_EQ(nested.atlas_calls, 0);
        EXPECT_EQ(outer.memory.outstanding.load(), 0u);
    }
}

TEST(
    font_bake,
    real_truetype_allocation_failures_release_scratch_and_precede_upload
) {
    const auto* path = std::getenv("CHERYL_STB_ALLOCATION_TTF");
    if (!path || !*path)
        GTEST_SKIP() << "Set CHERYL_STB_ALLOCATION_TTF to a TrueType acceptance face";
    reject_each_stb_allocation(path);
}

TEST(
    font_bake,
    real_cff_allocation_failures_release_scratch_and_precede_upload
) {
    const auto* path = std::getenv("CHERYL_STB_ALLOCATION_CFF");
    if (!path || !*path)
        GTEST_SKIP() << "Set CHERYL_STB_ALLOCATION_CFF to a CFF OpenType acceptance face";
    reject_each_stb_allocation(path);
}

TEST(
    font_bake,
    incomplete_bakes_retry_a_cleared_larger_atlas_and_keep_the_successful_pixels
) {
    for (const int incomplete : {0, -3}) {
        SCOPED_TRACE(incomplete);
        auto memory = std::make_shared<CE::Testing::FailingMemoryResource>();
        std::vector<int> sizes;
        {
            const auto atlas = FontDetail::bake_font_atlas(
                "fixture.ttf",
                [&](std::span<unsigned char> pixels, const int size) {
                    sizes.push_back(size);
                    EXPECT_EQ(pixels.size(), static_cast<std::size_t>(size) * size);
                    EXPECT_TRUE(std::all_of(pixels.begin(), pixels.end(), [](const auto value) { return value == 0; }));
                    pixels.front() = 7;
                    pixels.back() = 9;
                    return sizes.size() == 1 ? incomplete : 1;
                },
                std::pmr::polymorphic_allocator<unsigned char>{memory.get()}
            );
            EXPECT_EQ(sizes, (std::vector<int>{256, 512}));
            EXPECT_EQ(atlas.size, 512);
            EXPECT_EQ(atlas.pixels.front(), 7);
            EXPECT_EQ(atlas.pixels.back(), 9);
            EXPECT_GT(memory->outstanding.load(), 0u);
        }
        EXPECT_EQ(memory->outstanding.load(), 0u);
    }
}

TEST(
    font_bake,
    reaching_the_atlas_limit_rejects_partial_data_and_releases_storage
) {
    auto memory = std::make_shared<CE::Testing::FailingMemoryResource>();
    std::vector<int> sizes;
    const auto bake = [&](std::span<unsigned char> pixels, const int size) {
        sizes.push_back(size);
        pixels.front() = 7;
        return -3;
    };
    EXPECT_THROW((void)FontDetail::bake_font_atlas("fixture.ttf", bake, std::pmr::polymorphic_allocator<unsigned char>{memory.get()}),
        CE::Exceptions::runtime_exception);
    EXPECT_EQ(sizes, (std::vector<int>{256, 512, 1024, 2048, 4096}));
    EXPECT_EQ(memory->outstanding.load(), 0u);
}

TEST(
    font_bake,
    rejected_cpu_allocation_precedes_the_baker_and_releases_no_unowned_storage
) {
    auto memory = std::make_shared<CE::Testing::FailingMemoryResource>();
    memory->reject_next();
    int calls = 0;
    const auto bake = [&](std::span<unsigned char>, int) {
        ++calls;
        return 1;
    };
    EXPECT_THROW((void)FontDetail::bake_font_atlas("fixture.ttf", bake, std::pmr::polymorphic_allocator<unsigned char>{memory.get()}),
        std::bad_alloc);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(memory->rejected.load(), 1u);
    EXPECT_EQ(memory->outstanding.load(), 0u);
}

TEST(
    font_upload,
    invalid_size_and_missing_or_empty_files_never_start_resource_uploads
) {
    const TemporaryDirectory directory;
    const auto empty = directory.path / "empty.ttf";
    std::ofstream(empty, std::ios::binary).close();
    FontUploadProvider provider;
    EXPECT_THROW((void)STBFont::load_font(empty, 0, provider), CE::Exceptions::invalid_args);
    EXPECT_THROW((void)STBFont::load_font(directory.path / "missing.ttf", 16, provider), CE::Exceptions::runtime_exception);
    EXPECT_THROW((void)STBFont::load_font(empty, 16, provider), CE::Exceptions::runtime_exception);
    EXPECT_EQ(provider.geometry_calls, 0);
    EXPECT_EQ(provider.atlas_calls, 0);
}

TEST(
    font_upload,
    rejected_geometry_releases_cpu_storage_without_starting_the_atlas
) {
    FontUploadProvider provider;
    provider.reject_geometry = true;
    auto vertices = glyph_vertices(provider);
    const std::array<unsigned char, 3> alpha{1, 2, 3};
    const std::array<float, font_character_count> advances{};
    EXPECT_THROW((void)FontDetail::upload_baked_font(provider, std::move(vertices), alpha, PixelSize{3, 1}, advances, 12),
        CE::Exceptions::failed_operation);
    EXPECT_TRUE(provider.cpu_vertices.expired());
    EXPECT_TRUE(provider.uploaded_geometry.expired());
    EXPECT_EQ(provider.geometry_calls, 1);
    EXPECT_EQ(provider.atlas_calls, 0);
}

TEST(
    font_upload,
    atlas_throw_or_null_releases_the_completed_glyph_geometry
) {
    for (const auto outcome : {FontUploadProvider::AtlasResult::Throw, FontUploadProvider::AtlasResult::Null}) {
        SCOPED_TRACE(outcome == FontUploadProvider::AtlasResult::Throw ? "atlas throws" : "atlas returns null");
        FontUploadProvider provider;
        provider.atlas_result = outcome;
        auto vertices = glyph_vertices(provider);
        const std::array<unsigned char, 3> alpha{1, 2, 3};
        const std::array<float, font_character_count> advances{};
        if (outcome == FontUploadProvider::AtlasResult::Throw) {
            EXPECT_THROW((void)FontDetail::upload_baked_font(provider, std::move(vertices), alpha, PixelSize{3, 1}, advances, 12),
                CE::Exceptions::failed_operation);
        } else {
            EXPECT_THROW((void)STBFont(FontDetail::upload_baked_font(provider, std::move(vertices), alpha, PixelSize{3, 1}, advances, 12)),
                CE::Exceptions::invalid_args);
        }
        EXPECT_TRUE(provider.cpu_vertices.expired());
        EXPECT_TRUE(provider.uploaded_geometry.expired());
        EXPECT_TRUE(provider.uploaded_atlas.expired());
        EXPECT_EQ(provider.geometry_calls, 1);
        EXPECT_EQ(provider.atlas_calls, 1);
    }
}

TEST(
    font_upload,
    success_retains_backend_copies_and_metrics_after_cpu_release
) {
    FontUploadProvider provider;
    auto vertices = glyph_vertices(provider);
    std::array<unsigned char, 3> alpha{1, 2, 3};
    std::array<float, font_character_count> advances{};
    advances.fill(2);
    advances['A' - first_font_character] = 5;
    auto font =
        std::make_shared<STBFont>(FontDetail::upload_baked_font(provider, std::move(vertices), alpha, PixelSize{3, 1}, advances, 12));
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
