#include <assets/types/2d/unicode-text.h>
#include <assets/submission/draw2d.h>
#include <assets/resources/resource-provider.h>
#include <assets/resources/sampler.h>
#include <core/rendering/render-frame.h>
#include <ext/matrix_clip_space.hpp>
#include <ext/matrix_transform.hpp>
#include <gtest/gtest.h>
#include <internals/exceptions.h>
#include <testing/material-builder.h>

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
    using namespace CE::Text;

    CE::Text::FontCollection fonts() {
        CE::Text::FontSelection selection;
        selection.automatic_system_fonts = false;
        return CE::Text::FontCollection::load(selection);
    }
    enum class GeometryFault { None, Null, Count, Layout, Topology };
    struct TextGeometry final : CE::Rendering::Geometry::Buffer2D {
        std::vector<CE::Rendering::Geometry::Vertex2D> vertices;
        CE::Rendering::Geometry::Layout2D selected_layout = CE::Rendering::Geometry::Layout2D::Position3UV2;
        CE::Rendering::Geometry::Topology selected_topology = CE::Rendering::Geometry::Topology::Triangles;
        explicit TextGeometry(std::span<const CE::Rendering::Geometry::Vertex2D> input)
        : vertices(input.begin(), input.end()) {}
        CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override { return selected_layout; }
        CE::Rendering::Geometry::Topology topology() const noexcept override { return selected_topology; }
        std::size_t vertex_count() const noexcept override { return vertices.size(); }
        void bind() const override { throw std::logic_error("CPU text submission must not bind geometry"); }
        void draw(std::size_t, std::size_t) const override { throw std::logic_error("CPU text submission must not draw"); }
    };
    struct TextImage final : CE::Rendering::Resources::Image {
        std::vector<unsigned char> alpha;
        CE::Rendering::Geometry::PixelSize size;
        TextImage(std::span<const unsigned char> input, CE::Rendering::Geometry::PixelSize size)
        : alpha(input.begin(), input.end()), size(size) {}
        CE::Rendering::Geometry::PixelSize pixel_size() const override { return size; }
        void bind(std::uint32_t) const override { throw std::logic_error("CPU text submission must not bind an atlas"); }
    };
    struct TextProvider final : CE::Rendering::Resources::Provider {
        CE::Testing::MaterialBuilder builder = CE::Testing::ordinary_materials();
        int calls = 0, fail_at = 0;
        bool bad_size = false;
        GeometryFault geometry_fault = GeometryFault::None;
        int geometry_fault_at = 0;
        std::vector<std::weak_ptr<CE::Rendering::Geometry::Buffer2D>> geometry;
        std::vector<std::weak_ptr<CE::Rendering::Resources::Image>> images;

        using CE::Rendering::Resources::Provider::upload_geometry;
        CE::Assets::Shaders::Builder* shader_asset_builder() noexcept override { return &builder; }
        void admit() {
            if (++calls == fail_at)
                throw std::runtime_error("text upload");
        }
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded&) override {
            throw std::logic_error("Unexpected RGBA upload");
        }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char> alpha, CE::Rendering::Geometry::PixelSize size) override {
            admit();
            if (bad_size)
                ++size.width;
            auto result = std::make_shared<TextImage>(alpha, size);
            images.push_back(result);
            return result;
        }
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D> vertices, CE::Rendering::Geometry::Topology) override {
            admit();
            if (calls == geometry_fault_at && geometry_fault == GeometryFault::Null)
                return {};
            auto result = std::make_shared<TextGeometry>(vertices);
            if (calls == geometry_fault_at) {
                if (geometry_fault == GeometryFault::Count)
                    result->vertices.pop_back();
                if (geometry_fault == GeometryFault::Layout)
                    result->selected_layout = CE::Rendering::Geometry::Layout2D::Position3UV2Color4;
                if (geometry_fault == GeometryFault::Topology)
                    result->selected_topology = CE::Rendering::Geometry::Topology::TriangleStrip;
            }
            geometry.push_back(result);
            return result;
        }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override { return {}; }
    };
    class TextPipeline final : public CE::Rendering::Resources::Pipeline {
        static CE::Rendering::Pipelines::Definition definition() {
            CE::Rendering::Pipelines::Definition result;
            result.program_sources = {"text.vert", "text.frag"};
            result.parameters = {
                {"model", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Model},
                {"projection", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Projection},
                {"alpha", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Alpha},
                {"scale", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Scale},
                {"image", CE::Rendering::Parameters::Type::Sampler2D}
            };
            return result;
        }

    public:
        TextPipeline()
        : CE::Rendering::Resources::Pipeline(definition()) {}
        explicit TextPipeline(CE::Rendering::Pipelines::Definition value)
        : CE::Rendering::Resources::Pipeline(std::move(value)) {}
    };
    class TextSampler final : public CE::Rendering::Sampling::Sampler {
    public:
        TextSampler()
        : CE::Rendering::Sampling::Sampler({}, 1) {}
        void bind(std::uint32_t) const override { throw std::logic_error("CPU text submission must not bind a sampler"); }
    };
    CE::Rendering::DrawStyle2D style() {
        CE::Rendering::DrawStyle2D result;
        result.material = std::make_shared<CE::Rendering::Resources::Material>(
            CE::Rendering::Materials::Definition{std::make_shared<TextPipeline>(), {}}
        );
        result.scale = 2;
        result.model_matrix[3][0] = 5;
        return result;
    }
    CE::Rendering::Submission::Context2D context() {
        CE::Rendering::Submission::Context2D result;
        result.image = CE::Rendering::Submission::ImageParameter2D{"image", 2};
        return result;
    }

    void expect_same_vertices(
        const CE::Text::Rendering::Generation& text,
        const CE::Rendering::DrawStyle2D& selected,
        const CE::Rendering::Submission::Context2D& submission
    ) {
        const auto glyphs = CE::Rendering::Submission::resolve_text(text, selected, submission);
        const auto runs = CE::Rendering::Submission::resolve_text_runs(text, selected, submission);
        ASSERT_FALSE(runs.empty());
        const auto geometry = std::dynamic_pointer_cast<TextGeometry>(text.positioned_geometry());
        ASSERT_TRUE(geometry);
        ASSERT_EQ(geometry->vertices.size(), glyphs.size() * 6);
        const auto run_model = std::get<glm::mat4>(runs.front().parameters.at("model"));
        const auto camera = submission.pass.projection * submission.pass.view;
        for (std::size_t glyph = 0; glyph < glyphs.size(); ++glyph) {
            const auto& packet = glyphs[glyph];
            const auto source = std::dynamic_pointer_cast<const TextGeometry>(packet.geometry);
            ASSERT_TRUE(source);
            const auto glyph_model = std::get<glm::mat4>(packet.parameters.at("model"));
            for (std::size_t index = 0; index < 6; ++index) {
                const auto& original = source->vertices[packet.first_vertex + index];
                const auto& positioned = geometry->vertices[glyph * 6 + index];
                const auto expected = camera * glyph_model *
                                      glm::vec4(original.x * selected.scale, original.y * selected.scale, original.z * selected.scale, 1);
                const auto actual =
                    camera * run_model *
                    glm::vec4(positioned.x * selected.scale, positioned.y * selected.scale, positioned.z * selected.scale, 1);
                for (glm::length_t component = 0; component < 4; ++component)
                    EXPECT_NEAR(actual[component], expected[component], 0.0001f);
                EXPECT_FLOAT_EQ(positioned.u, original.u);
                EXPECT_FLOAT_EQ(positioned.v, original.v);
            }
        }
    }
}

TEST(text_resources, prepare) {
    const auto prepared = CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "A A"));
    ASSERT_EQ(prepared.pages().size(), 1u);
    ASSERT_EQ(prepared.draws().size(), 2u);
    EXPECT_EQ(prepared.draws()[0].index, prepared.draws()[1].index);
    const auto& page = prepared.pages()[0];
    EXPECT_EQ(page.vertices.size(), 6u); // Repeated glyphs share their raster/quad within a generation.
    EXPECT_EQ(page.alpha.size(), static_cast<std::size_t>(page.size.width) * page.size.height);
    EXPECT_LT(page.size.height, 1024u);
    EXPECT_TRUE(std::ranges::any_of(page.alpha, [](auto alpha) { return alpha != 0; }));
    for (std::uint32_t row = 0; row < page.size.height; ++row) {
        EXPECT_EQ(page.alpha[static_cast<std::size_t>(row) * page.size.width], 0);
        EXPECT_EQ(page.alpha[static_cast<std::size_t>(row + 1) * page.size.width - 1], 0);
    }
    for (std::uint32_t column = 0; column < page.size.width; ++column) {
        EXPECT_EQ(page.alpha[column], 0);
        EXPECT_EQ(page.alpha[static_cast<std::size_t>(page.size.height - 1) * page.size.width + column], 0);
    }
    for (const auto& vertex : page.vertices) {
        EXPECT_GT(vertex.u, 0);
        EXPECT_LT(vertex.u, 1);
        EXPECT_GT(vertex.v, 0);
        EXPECT_LT(vertex.v, 1);
    }
    TextProvider provider;
    const auto uploaded = CE::Text::Rendering::upload(prepared, provider);
    ASSERT_EQ(uploaded.pages().size(), 1u);
    const auto image = std::dynamic_pointer_cast<TextImage>(uploaded.pages()[0].atlas);
    ASSERT_TRUE(image);
    EXPECT_EQ(image->alpha, page.alpha);
    EXPECT_EQ(provider.calls, 2);
}

TEST(text_resources, defaults) {
    TextProvider provider;
    const auto prepared = CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "AA"), {.positioned_runs = true});
    const auto uploaded = CE::Text::Rendering::upload(prepared, provider);
    ASSERT_TRUE(uploaded.material());
    const auto packets = CE::Rendering::Submission::resolve_text(uploaded, {}, {});
    const auto runs = CE::Rendering::Submission::resolve_text_runs(uploaded, {}, {});
    ASSERT_FALSE(packets.empty());
    ASSERT_FALSE(runs.empty());
    EXPECT_EQ(packets.front().material, uploaded.material());
    EXPECT_EQ(runs.front().material, uploaded.material());
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(packets.front().parameters.at("image")).unit, 0u);
    ASSERT_EQ(provider.builder.requests.size(), 1u);
    EXPECT_EQ(provider.builder.requests.front().first, CE::Rendering::Materials::Builtin2D::Textured);
    EXPECT_EQ(provider.builder.requests.front().second, CE::Rendering::Pipelines::Blend::StraightAlpha);
    EXPECT_TRUE(uploaded.material()->definition().sampling.empty());

    const auto custom = style().material;
    const auto overridden = CE::Text::Rendering::upload(prepared, provider, custom);
    EXPECT_EQ(provider.builder.requests.size(), 1u);
    EXPECT_EQ(CE::Rendering::Submission::resolve_text(overridden, {}, {}).front().material, custom);
    auto per_draw = style();
    EXPECT_EQ(CE::Rendering::Submission::resolve_text(uploaded, per_draw, {}).front().material, per_draw.material);
    EXPECT_EQ(uploaded.material(), packets.front().material);
}

TEST(text_resources, default_failure) {
    TextProvider provider;
    provider.builder.missing = true;
    const auto prepared = CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "A"));
    EXPECT_THROW(static_cast<void>(CE::Text::Rendering::upload(prepared, provider)), CE::Exceptions::failed_operation);
    EXPECT_EQ(provider.calls, 0);
    const auto custom = style().material;
    const auto uploaded = CE::Text::Rendering::upload(prepared, provider, custom);
    EXPECT_EQ(uploaded.material(), custom);
}

TEST(text_resources, pages) {
    CE::Text::LayoutOptions options;
    options.pixel_height = 16;
    const auto prepared =
        CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", options), {32});
    ASSERT_GT(prepared.pages().size(), 1u);
    TextProvider provider;
    const auto uploaded = CE::Text::Rendering::upload(prepared, provider);
    const auto packets = CE::Rendering::Submission::resolve_text(uploaded, style(), context());
    ASSERT_EQ(packets.size(), uploaded.draws().size());
    for (std::size_t index = 0; index < packets.size(); ++index) {
        const auto& draw = uploaded.draws()[index];
        EXPECT_EQ(packets[index].geometry, uploaded.pages()[draw.page].geometry);
        EXPECT_EQ(
            std::get<CE::Rendering::Parameters::ImageBinding>(packets[index].parameters.at("image")).image,
            uploaded.pages()[draw.page].atlas
        );
        EXPECT_EQ(packets[index].first_vertex, draw.index * 6);
        EXPECT_EQ(packets[index].vertex_count, 6u);
        const auto model = std::get<glm::mat4>(packets[index].parameters.at("model"));
        EXPECT_FLOAT_EQ(model[3][0], 5 + draw.x * 2);
        EXPECT_FLOAT_EQ(model[3][1], draw.y * 2);
    }
}

TEST(text_resources, replacement) {
    TextProvider provider;
    auto active = std::make_shared<CE::Text::Rendering::Generation>(
        CE::Text::Rendering::upload(CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "AA")), provider)
    );
    auto packets = CE::Rendering::Submission::resolve_text(*active, style(), context());
    const auto old_geometry = std::weak_ptr(active->pages()[0].geometry);
    const auto old_atlas = std::weak_ptr(active->pages()[0].atlas);
    const auto old_text = std::weak_ptr(active);
    provider.calls = 0;
    provider.fail_at = 2;
    EXPECT_THROW(
        {
            auto next = std::make_shared<CE::Text::Rendering::Generation>(
                CE::Text::Rendering::upload(CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "B")), provider)
            );
            active = std::move(next);
        },
        std::runtime_error
    );
    EXPECT_EQ(active, old_text.lock());
    EXPECT_TRUE(provider.geometry.back().expired()); // The failed candidate publishes no owner.
    provider.fail_at = 0;
    active = std::make_shared<CE::Text::Rendering::Generation>(
        CE::Text::Rendering::upload(CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "B")), provider)
    );
    EXPECT_TRUE(old_text.expired());
    EXPECT_FALSE(old_geometry.expired());
    EXPECT_FALSE(old_atlas.expired());
    EXPECT_NE(active->pages()[0].geometry, packets[0].geometry);
    EXPECT_EQ(packets[0].geometry, old_geometry.lock());
    packets.clear();
    EXPECT_TRUE(old_geometry.expired());
    EXPECT_TRUE(old_atlas.expired());
}

TEST(text_resources, empty) {
    TextProvider provider;
    const auto prepared = CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), " \t\n"));
    EXPECT_TRUE(prepared.pages().empty());
    const auto uploaded = CE::Text::Rendering::upload(prepared, provider);
    EXPECT_TRUE(CE::Rendering::Submission::resolve_text(uploaded, style(), context()).empty());
    EXPECT_EQ(provider.calls, 0);
    EXPECT_TRUE(CE::Rendering::Submission::resolve_text(uploaded, {}, {}).empty());
    EXPECT_THROW(
        static_cast<void>(CE::Rendering::Submission::resolve_text(uploaded, style(), {.image = std::nullopt})), CE::Exceptions::invalid_args
    );
}

TEST(text_resources, failures) {
    const auto shaped = CE::Text::layout_text(fonts(), "A");
    EXPECT_THROW(static_cast<void>(CE::Text::Rendering::prepare(shaped, {2})), CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(CE::Text::Rendering::prepare(shaped, {4})), CE::Exceptions::runtime_exception);
    TextProvider provider;
    provider.bad_size = true;
    EXPECT_THROW(
        static_cast<void>(CE::Text::Rendering::upload(CE::Text::Rendering::prepare(shaped), provider)), CE::Exceptions::failed_operation
    );
    EXPECT_TRUE(provider.geometry.back().expired());
    EXPECT_TRUE(provider.images.back().expired());
    provider.bad_size = false;
    const auto uploaded = CE::Text::Rendering::upload(CE::Text::Rendering::prepare(shaped), provider);
    auto collision = style();
    collision.parameters.emplace("image", CE::Rendering::Parameters::ImageBinding{uploaded.pages()[0].atlas, 0});
    EXPECT_THROW(static_cast<void>(CE::Rendering::Submission::resolve_text(uploaded, collision, context())), CE::Exceptions::invalid_args);
}

TEST(text_runs, opt_in) {
    const auto shaped = CE::Text::layout_text(fonts(), "AA");
    const auto plain = CE::Text::Rendering::prepare(shaped);
    EXPECT_FALSE(plain.has_positioned_runs());
    EXPECT_TRUE(plain.positioned_vertices().empty());
    EXPECT_TRUE(plain.runs().empty());
    TextProvider provider;
    const auto plain_upload = CE::Text::Rendering::upload(plain, provider);
    EXPECT_FALSE(plain_upload.positioned_geometry());
    EXPECT_EQ(provider.calls, 2);
    EXPECT_THROW(
        static_cast<void>(CE::Rendering::Submission::resolve_text_runs(plain_upload, style(), context())), CE::Exceptions::invalid_args
    );
    const auto prepared = CE::Text::Rendering::prepare(shaped, {.positioned_runs = true});
    ASSERT_TRUE(prepared.has_positioned_runs());
    ASSERT_EQ(prepared.pages().size(), 1u);
    EXPECT_EQ(prepared.pages()[0].vertices.size(), 6u);
    EXPECT_EQ(prepared.positioned_vertices().size(), 12u);
    ASSERT_EQ(prepared.runs().size(), 1u);
    EXPECT_EQ(prepared.runs()[0].vertex_count, 12u);
    provider.calls = 0;
    const auto uploaded = CE::Text::Rendering::upload(prepared, provider);
    EXPECT_EQ(provider.calls, 3);
    EXPECT_EQ(CE::Rendering::Submission::resolve_text(uploaded, style(), context()).size(), 2u);
    const auto packets = CE::Rendering::Submission::resolve_text_runs(uploaded, style(), context());
    ASSERT_EQ(packets.size(), 1u);
    EXPECT_EQ(packets[0].first_vertex, 0u);
    EXPECT_EQ(packets[0].vertex_count, 12u);
}

TEST(text_runs, transforms) {
    TextProvider provider;
    const auto uploaded = CE::Text::Rendering::upload(
        CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "A A\nAA"), {.positioned_runs = true}), provider
    );
    auto selected = style();
    selected.model_matrix = glm::translate(glm::mat4{1}, {17, -5, 0});
    selected.model_matrix = glm::rotate(selected.model_matrix, 0.63f, {0, 0, 1});
    selected.model_matrix = glm::scale(selected.model_matrix, {1.3f, 0.7f, 1});
    auto submission = context();
    submission.pass.projection = glm::ortho(-200.0f, 200.0f, -200.0f, 200.0f);
    submission.pass.view = glm::translate(glm::mat4{1}, {-9, 11, 0});
    for (const auto scale : std::array{-1.75f, 0.0f, 1.0f, 2.5f}) {
        SCOPED_TRACE(scale);
        selected.scale = scale;
        expect_same_vertices(uploaded, selected, submission);
    }
}

TEST(text_runs, pages) {
    CE::Text::LayoutOptions options;
    options.pixel_height = 16;
    const auto prepared = CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "AAZZAA", options), {20, true});
    ASSERT_EQ(prepared.pages().size(), 2u);
    ASSERT_EQ(prepared.runs().size(), 3u);
    TextProvider provider;
    const auto uploaded = CE::Text::Rendering::upload(prepared, provider);
    const auto packets = CE::Rendering::Submission::resolve_text_runs(uploaded, style(), context());
    ASSERT_EQ(packets.size(), 3u);
    for (std::size_t index = 0; index < packets.size(); ++index) {
        const auto page = index == 1 ? 1u : 0u;
        EXPECT_EQ(uploaded.runs()[index].page, page);
        EXPECT_EQ(packets[index].geometry, uploaded.positioned_geometry());
        EXPECT_EQ(packets[index].first_vertex, index * 12);
        EXPECT_EQ(packets[index].vertex_count, 12u);
        const auto image = std::get<CE::Rendering::Parameters::ImageBinding>(packets[index].parameters.at("image"));
        EXPECT_EQ(image.image, uploaded.pages()[page].atlas);
        EXPECT_EQ(image.unit, 2u);
    }
    expect_same_vertices(uploaded, style(), context());
}

TEST(text_runs, unicode) {
    constexpr std::u8string_view sample = u8"e\u0301 \u4e2d\n\u05d0\u05d1\u05d2 123\nEnglish wraps across several lines";
    CE::Text::LayoutOptions options;
    options.maximum_width = 70;
    const auto shaped = CE::Text::layout_text(fonts(), {reinterpret_cast<const char*>(sample.data()), sample.size()}, options);
    ASSERT_TRUE(std::ranges::any_of(shaped.glyphs(), [](const auto& glyph) { return glyph.missing; }));
    ASSERT_TRUE(std::ranges::any_of(shaped.lines(), [](const auto& line) { return line.right_to_left; }));
    ASSERT_GT(shaped.lines().size(), 3u);
    TextProvider provider;
    const auto uploaded = CE::Text::Rendering::upload(CE::Text::Rendering::prepare(shaped, {.positioned_runs = true}), provider);
    expect_same_vertices(uploaded, style(), context());
}

TEST(text_runs, bindings) {
    CE::Text::LayoutOptions options;
    options.pixel_height = 16;
    TextProvider provider;
    const auto uploaded =
        CE::Text::Rendering::upload(CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "AZA", options), {20, true}), provider);
    auto selected = style();
    auto definition = selected.material->definition().pipeline->definition();
    definition.parameters.push_back({"overlay", CE::Rendering::Parameters::Type::Sampler2D});
    definition.parameters.push_back({"tint", CE::Rendering::Parameters::Type::Vec4});
    const auto sampler = std::make_shared<TextSampler>();
    selected.material = std::make_shared<CE::Rendering::Resources::Material>(CE::Rendering::Materials::Definition{
        std::make_shared<TextPipeline>(std::move(definition)), {{"tint", glm::vec4{0.5f}}}, {{"image", sampler}}
    });
    selected.parameters.emplace("overlay", CE::Rendering::Parameters::ImageBinding{uploaded.pages()[0].atlas, 7, sampler});
    selected.parameters.emplace("tint", glm::vec4{1, 0, 0, 1});
    selected.alpha = 0.35f;
    selected.scale = -0.5f;
    selected.clip = CE::Rendering::ClipRegion2D{{4, 8, 40, 50}, 64, 64};
    auto submission = context();
    submission.parameters.emplace("tint", glm::vec4{0.25f});
    const auto packets = CE::Rendering::Submission::resolve_text_runs(uploaded, selected, submission);
    ASSERT_EQ(packets.size(), 3u);
    for (const auto& packet : packets) {
        EXPECT_EQ(packet.material, selected.material);
        EXPECT_EQ(packet.clip, selected.clip);
        EXPECT_EQ(std::get<glm::mat4>(packet.parameters.at("model")), selected.model_matrix);
        EXPECT_EQ(std::get<float>(packet.parameters.at("scale")), selected.scale);
        EXPECT_EQ(std::get<float>(packet.parameters.at("alpha")), selected.alpha);
        EXPECT_EQ(std::get<glm::vec4>(packet.parameters.at("tint")), (glm::vec4{1, 0, 0, 1}));
        const auto image = std::get<CE::Rendering::Parameters::ImageBinding>(packet.parameters.at("image"));
        EXPECT_EQ(image.unit, 2u);
        EXPECT_EQ(image.sampler, sampler);
        const auto overlay = std::get<CE::Rendering::Parameters::ImageBinding>(packet.parameters.at("overlay"));
        EXPECT_EQ(overlay.image, uploaded.pages()[0].atlas);
        EXPECT_EQ(overlay.unit, 7u);
        EXPECT_EQ(overlay.sampler, sampler);
    }
    CE::Rendering::Frame frame;
    CE::Rendering::FrameWriter writer(frame);
    auto pass = writer.begin_pass(submission.pass.projection, submission.pass.view);
    pass.add(packets);
    selected.clip = CE::Rendering::ClipRegion2D{{10, 12, 20, 30}, 64, 64};
    selected.alpha = 0.75f;
    submission.image->unit = 4;
    pass.add(CE::Rendering::Submission::resolve_text_runs(uploaded, selected, submission));
    ASSERT_EQ(frame.passes()[0].draws.size(), 6u);
    for (std::size_t index = 0; index < 6; ++index) {
        const auto& packet = frame.passes()[0].draws[index];
        EXPECT_EQ(packet.authored_order, index);
        EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(packet.parameters.at("image")).unit, index < 3 ? 2u : 4u);
        EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("alpha")), index < 3 ? 0.35f : 0.75f);
    }
    EXPECT_NE(frame.passes()[0].draws[0].clip, frame.passes()[0].draws[3].clip);
}

TEST(text_runs, empty) {
    TextProvider provider;
    const auto prepared = CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), " \t\n"), {.positioned_runs = true});
    EXPECT_TRUE(prepared.has_positioned_runs());
    EXPECT_TRUE(prepared.positioned_vertices().empty());
    EXPECT_TRUE(prepared.runs().empty());
    const auto uploaded = CE::Text::Rendering::upload(prepared, provider);
    EXPECT_TRUE(uploaded.has_positioned_runs());
    EXPECT_FALSE(uploaded.positioned_geometry());
    EXPECT_TRUE(CE::Rendering::Submission::resolve_text_runs(uploaded, style(), context()).empty());
    EXPECT_EQ(provider.calls, 0);
    EXPECT_THROW(
        static_cast<void>(CE::Rendering::Submission::resolve_text_runs(uploaded, style(), {.image = std::nullopt})),
        CE::Exceptions::invalid_args
    );
}

TEST(text_runs, failures) {
    TextProvider provider;
    const auto prepared = CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "AA"), {.positioned_runs = true});
    provider.fail_at = 3;
    EXPECT_THROW(static_cast<void>(CE::Text::Rendering::upload(prepared, provider)), std::runtime_error);
    EXPECT_TRUE(std::ranges::all_of(provider.geometry, [](const auto& handle) { return handle.expired(); }));
    EXPECT_TRUE(std::ranges::all_of(provider.images, [](const auto& handle) { return handle.expired(); }));
    provider.fail_at = 0;
    const auto uploaded = CE::Text::Rendering::upload(prepared, provider);
    auto selected = style();
    selected.parameters.emplace("image", CE::Rendering::Parameters::ImageBinding{uploaded.pages()[0].atlas, 0});
    EXPECT_THROW(
        static_cast<void>(CE::Rendering::Submission::resolve_text_runs(uploaded, selected, context())), CE::Exceptions::invalid_args
    );
    selected = style();
    selected.clip = CE::Rendering::ClipRegion2D{{20, 0, 10, 10}, 64, 64};
    EXPECT_THROW(
        static_cast<void>(CE::Rendering::Submission::resolve_text_runs(uploaded, selected, context())), CE::Exceptions::invalid_args
    );
    auto submission = context();
    submission.constraints.depth = CE::Rendering::Pipelines::Depth::Less;
    EXPECT_THROW(
        static_cast<void>(CE::Rendering::Submission::resolve_text_runs(uploaded, style(), submission)), CE::Exceptions::invalid_args
    );
}

TEST(text_runs, malformed) {
    const auto prepared = CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "A"), {.positioned_runs = true});
    for (const auto fault : std::array{GeometryFault::Null, GeometryFault::Count, GeometryFault::Layout, GeometryFault::Topology}) {
        SCOPED_TRACE(static_cast<int>(fault));
        TextProvider provider;
        provider.geometry_fault_at = 3;
        provider.geometry_fault = fault;
        EXPECT_THROW(static_cast<void>(CE::Text::Rendering::upload(prepared, provider)), CE::Exceptions::failed_operation);
        EXPECT_TRUE(std::ranges::all_of(provider.geometry, [](const auto& handle) { return handle.expired(); }));
        EXPECT_TRUE(std::ranges::all_of(provider.images, [](const auto& handle) { return handle.expired(); }));
    }
}

TEST(text_runs, replacement) {
    TextProvider provider;
    auto active = std::make_shared<CE::Text::Rendering::Generation>(
        CE::Text::Rendering::upload(CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "AA"), {.positioned_runs = true}), provider)
    );
    const std::weak_ptr<CE::Rendering::Geometry::Buffer2D> positioned = active->positioned_geometry();
    const std::weak_ptr<CE::Rendering::Geometry::Buffer2D> glyphs = active->pages()[0].geometry;
    const std::weak_ptr<CE::Rendering::Resources::Image> atlas = active->pages()[0].atlas;
    const std::weak_ptr<CE::Text::Rendering::Generation> old_text = active;
    CE::Rendering::Frame frame;
    CE::Rendering::FrameWriter writer(frame);
    auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
    pass.add(CE::Rendering::Submission::resolve_text_runs(*active, style(), context()));
    provider.calls = 0;
    provider.fail_at = 3;
    EXPECT_THROW(
        {
            auto next = std::make_shared<CE::Text::Rendering::Generation>(CE::Text::Rendering::upload(
                CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "B"), {.positioned_runs = true}), provider
            ));
            active = std::move(next);
        },
        std::runtime_error
    );
    EXPECT_EQ(active, old_text.lock());
    provider.fail_at = 0;
    active = std::make_shared<CE::Text::Rendering::Generation>(
        CE::Text::Rendering::upload(CE::Text::Rendering::prepare(CE::Text::layout_text(fonts(), "B"), {.positioned_runs = true}), provider)
    );
    EXPECT_TRUE(old_text.expired());
    EXPECT_TRUE(glyphs.expired());
    EXPECT_FALSE(positioned.expired());
    EXPECT_FALSE(atlas.expired());
    EXPECT_EQ(frame.passes()[0].draws[0].geometry, positioned.lock());
    EXPECT_NE(active->positioned_geometry(), positioned.lock());
    frame.recycle();
    EXPECT_TRUE(positioned.expired());
    EXPECT_TRUE(atlas.expired());
}
