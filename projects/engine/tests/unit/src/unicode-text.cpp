#include <assets/types/2d/unicode-text.h>
#include <assets/submission/draw2d.h>
#include <assets/resources/resource-provider.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
    using namespace CE::Assets;
    using namespace CE::Text;
    using namespace CE::RenderAPIs;

    FontCollection fonts() {
        FontSelection selection;
        selection.automatic_system_fonts = false;
        return FontCollection::load(selection);
    }
    struct TextGeometry final : Geometry2D {
        std::vector<CE::Vertex2D> vertices;
        explicit TextGeometry(std::span<const CE::Vertex2D> input)
        : vertices(input.begin(), input.end()) {}
        VertexLayout2D vertex_layout() const noexcept override { return VertexLayout2D::Position3UV2; }
        PrimitiveTopology topology() const noexcept override { return PrimitiveTopology::Triangles; }
        std::size_t vertex_count() const noexcept override { return vertices.size(); }
        void bind() const override { throw std::logic_error("CPU text submission must not bind geometry"); }
        void draw(std::size_t, std::size_t) const override { throw std::logic_error("CPU text submission must not draw"); }
    };
    struct TextImage final : Image {
        std::vector<unsigned char> alpha;
        PixelSize size;
        TextImage(std::span<const unsigned char> input, PixelSize size)
        : alpha(input.begin(), input.end()), size(size) {}
        PixelSize pixel_size() const override { return size; }
        void bind(std::uint32_t) const override { throw std::logic_error("CPU text submission must not bind an atlas"); }
    };
    struct TextProvider final : ResourceProvider {
        int calls = 0, fail_at = 0;
        bool bad_size = false;
        std::vector<std::weak_ptr<Geometry2D>> geometry;
        std::vector<std::weak_ptr<Image>> images;

        using ResourceProvider::upload_geometry;
        void admit() {
            if (++calls == fail_at)
                throw std::runtime_error("text upload");
        }
        std::shared_ptr<Image> create_image(const DecodedImage&) override { throw std::logic_error("Unexpected RGBA upload"); }
        std::shared_ptr<Image> create_font_atlas(std::span<const unsigned char> alpha, PixelSize size) override {
            admit();
            if (bad_size)
                ++size.width;
            auto result = std::make_shared<TextImage>(alpha, size);
            images.push_back(result);
            return result;
        }
        std::shared_ptr<Geometry2D> upload_geometry(std::span<const CE::Vertex2D> vertices, PrimitiveTopology) override {
            admit();
            auto result = std::make_shared<TextGeometry>(vertices);
            geometry.push_back(result);
            return result;
        }
        std::shared_ptr<Shader> link_program(const std::vector<std::filesystem::path>&) override { return {}; }
    };
    class TextPipeline final : public Pipeline {
        static PipelineDefinition definition() {
            PipelineDefinition result;
            result.program_sources = {"text.vert", "text.frag"};
            result.parameters = {{"model", ParameterType::Mat4, true, ParameterSemantic::Model},
                {"projection", ParameterType::Mat4, true, ParameterSemantic::Projection},
                {"alpha", ParameterType::Float, true, ParameterSemantic::Alpha},
                {"scale", ParameterType::Float, true, ParameterSemantic::Scale}, {"image", ParameterType::Sampler2D}};
            return result;
        }

    public:
        TextPipeline()
        : Pipeline(definition()) {}
    };
    DrawStyle2D style() {
        DrawStyle2D result;
        result.material = std::make_shared<Material>(MaterialDefinition{std::make_shared<TextPipeline>(), {}});
        result.scale = 2;
        result.model_matrix[3][0] = 5;
        return result;
    }
    SubmissionContext2D context() {
        SubmissionContext2D result;
        result.image = ImageParameter2D{"image", 2};
        return result;
    }
}

TEST(text_resources, prepare) {
    const auto prepared = prepare_text(layout_text(fonts(), "A A"));
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
    const auto uploaded = upload_text(prepared, provider);
    ASSERT_EQ(uploaded.pages().size(), 1u);
    const auto image = std::dynamic_pointer_cast<TextImage>(uploaded.pages()[0].atlas);
    ASSERT_TRUE(image);
    EXPECT_EQ(image->alpha, page.alpha);
    EXPECT_EQ(provider.calls, 2);
}

TEST(text_resources, pages) {
    LayoutOptions options;
    options.pixel_height = 16;
    const auto prepared = prepare_text(layout_text(fonts(), "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", options), {32});
    ASSERT_GT(prepared.pages().size(), 1u);
    TextProvider provider;
    const auto uploaded = upload_text(prepared, provider);
    const auto packets = resolve_text(uploaded, style(), context());
    ASSERT_EQ(packets.size(), uploaded.draws().size());
    for (std::size_t index = 0; index < packets.size(); ++index) {
        const auto& draw = uploaded.draws()[index];
        EXPECT_EQ(packets[index].geometry, uploaded.pages()[draw.page].geometry);
        EXPECT_EQ(std::get<ImageBinding>(packets[index].parameters.at("image")).image, uploaded.pages()[draw.page].atlas);
        EXPECT_EQ(packets[index].first_vertex, draw.index * 6);
        EXPECT_EQ(packets[index].vertex_count, 6u);
        const auto model = std::get<glm::mat4>(packets[index].parameters.at("model"));
        EXPECT_FLOAT_EQ(model[3][0], 5 + draw.x * 2);
        EXPECT_FLOAT_EQ(model[3][1], draw.y * 2);
    }
}

TEST(text_resources, replacement) {
    TextProvider provider;
    auto active = std::make_shared<RenderedText>(upload_text(prepare_text(layout_text(fonts(), "AA")), provider));
    auto packets = resolve_text(*active, style(), context());
    const auto old_geometry = std::weak_ptr(active->pages()[0].geometry);
    const auto old_atlas = std::weak_ptr(active->pages()[0].atlas);
    const auto old_text = std::weak_ptr(active);
    provider.calls = 0;
    provider.fail_at = 2;
    EXPECT_THROW({
        auto next = std::make_shared<RenderedText>(upload_text(prepare_text(layout_text(fonts(), "B")), provider));
        active = std::move(next);
    }, std::runtime_error);
    EXPECT_EQ(active, old_text.lock());
    EXPECT_TRUE(provider.geometry.back().expired()); // The failed candidate publishes no owner.
    provider.fail_at = 0;
    active = std::make_shared<RenderedText>(upload_text(prepare_text(layout_text(fonts(), "B")), provider));
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
    const auto prepared = prepare_text(layout_text(fonts(), " \t\n"));
    EXPECT_TRUE(prepared.pages().empty());
    const auto uploaded = upload_text(prepared, provider);
    EXPECT_TRUE(resolve_text(uploaded, style(), context()).empty());
    EXPECT_EQ(provider.calls, 0);
    EXPECT_THROW(static_cast<void>(resolve_text(uploaded, style(), {})), CE::Exceptions::invalid_args);
}

TEST(text_resources, failures) {
    const auto shaped = layout_text(fonts(), "A");
    EXPECT_THROW(static_cast<void>(prepare_text(shaped, {2})), CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(prepare_text(shaped, {4})), CE::Exceptions::runtime_exception);
    TextProvider provider;
    provider.bad_size = true;
    EXPECT_THROW(static_cast<void>(upload_text(prepare_text(shaped), provider)), CE::Exceptions::failed_operation);
    EXPECT_TRUE(provider.geometry.back().expired());
    EXPECT_TRUE(provider.images.back().expired());
    provider.bad_size = false;
    const auto uploaded = upload_text(prepare_text(shaped), provider);
    auto collision = style();
    collision.parameters.emplace("image", ImageBinding{uploaded.pages()[0].atlas, 0});
    EXPECT_THROW(static_cast<void>(resolve_text(uploaded, collision, context())), CE::Exceptions::invalid_args);
}
