#include <gtest/gtest.h>

#include <ui/tgui/scene.h>
#include <internals/exceptions.h>

#include <array>
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {
    using namespace CE::UI::TGUI;

    struct MemoryImage final : CE::Assets::Image {
        CE::Assets::DecodedImage pixels;

        explicit MemoryImage(CE::Assets::DecodedImage image)
        : pixels(std::move(image)) {}
        CE::Assets::PixelSize pixel_size() const override { return pixels.size; }
        void bind(std::uint32_t) const override {}
    };

    struct MemoryGeometry final : CE::Assets::Geometry2D {
        std::vector<CE::Vertex2DColor> vertices;

        explicit MemoryGeometry(const std::span<const CE::Vertex2DColor> data)
        : vertices(data.begin(), data.end()) {}
        CE::Assets::VertexLayout2D vertex_layout() const noexcept override { return CE::Assets::VertexLayout2D::Position3UV2Color4; }
        CE::Assets::PrimitiveTopology topology() const noexcept override { return CE::Assets::PrimitiveTopology::Triangles; }
        std::size_t vertex_count() const noexcept override { return vertices.size(); }
        void bind() const override {}
        void draw(std::size_t, std::size_t) const override {}
    };

    struct MemoryProvider final : CE::Assets::ResourceProvider {
        int image_uploads = 0;
        int geometry_uploads = 0;
        int fail_geometry = 0;
        bool missing_image = false;
        bool wrong_size = false;
        std::vector<std::weak_ptr<MemoryImage>> images;
        std::vector<std::weak_ptr<MemoryGeometry>> geometry;

        std::shared_ptr<CE::Assets::Image> create_image(const CE::Assets::DecodedImage& data) override {
            ++image_uploads;
            if (missing_image)
                return {};
            auto image = std::make_shared<MemoryImage>(data);
            if (wrong_size)
                ++image->pixels.size.width;
            images.push_back(image);
            return image;
        }
        std::shared_ptr<CE::Assets::Image> create_font_atlas(std::span<const unsigned char>, CE::Assets::PixelSize) override {
            throw std::runtime_error("UI RGBA atlases use create_image");
        }
        std::shared_ptr<CE::Assets::Geometry2D> upload_geometry(std::span<const CE::Vertex2D>, CE::Assets::PrimitiveTopology) override {
            throw std::runtime_error("UI requires the colored layout");
        }
        std::shared_ptr<CE::Assets::Geometry2D>
        upload_geometry(const std::span<const CE::Vertex2DColor> vertices, const CE::Assets::PrimitiveTopology topology) override {
            ++geometry_uploads;
            if (fail_geometry == geometry_uploads)
                throw std::runtime_error("injected geometry failure");
            if (topology != CE::Assets::PrimitiveTopology::Triangles)
                throw std::runtime_error("UI requires triangles");
            auto buffer = std::make_shared<MemoryGeometry>(vertices);
            geometry.push_back(buffer);
            return buffer;
        }
        std::shared_ptr<CE::Assets::Shader> link_program(const std::vector<std::filesystem::path>&) override { return {}; }
    };

    class MemoryPipeline final : public CE::Assets::Pipeline {
    public:
        explicit MemoryPipeline(CE::Assets::PipelineDefinition definition)
        : Pipeline(std::move(definition)) {}
    };

    Materials materials(const bool compatible = true) {
        using namespace CE::Assets;
        PipelineDefinition definition;
        definition.program_sources = {"test-only-ui"};
        definition.vertex_layout = compatible ? VertexLayout2D::Position3UV2Color4 : VertexLayout2D::Position3UV2;
        definition.parameters = {{"projection", ParameterType::Mat4, true, ParameterSemantic::Projection},
                                 {"view", ParameterType::Mat4, true, ParameterSemantic::View},
                                 {"model", ParameterType::Mat4, true, ParameterSemantic::Model}};
        Materials result;
        result.solid = std::make_shared<Material>(MaterialDefinition{std::make_shared<MemoryPipeline>(definition), {}});
        definition.parameters.push_back({"pixels", ParameterType::Sampler2D});
        result.textured = std::make_shared<Material>(MaterialDefinition{std::make_shared<MemoryPipeline>(std::move(definition)), {}});
        result.image_parameter = "pixels";
        result.image_unit = 3;
        return result;
    }

    std::shared_ptr<Texture> texture(const unsigned char red = 255) {
        auto image = std::make_shared<Texture>(32);
        const std::array<unsigned char, 4> pixels{red, 0, 0, 255};
        image->loadTextureOnly({1, 1}, pixels.data(), true);
        return image;
    }

    RecordedScene recording(const std::shared_ptr<Texture>& image = {}, const std::size_t count = 1) {
        RenderTarget target;
        target.setView({0, 0, 320, 240}, {0, 0, 320, 240}, {320, 240});
        target.begin_recording();
        constexpr std::array<unsigned int, 3> indices{0, 1, 2};
        const std::array<tgui::Vertex, 3> vertices{tgui::Vertex{{0, 0}, {255, 255, 255, 128}, {0, 0}},
                                                   tgui::Vertex{{10, 0}, {255, 255, 255, 128}, {1, 0}},
                                                   tgui::Vertex{{0, 10}, {255, 255, 255, 128}, {0, 1}}};
        for (std::size_t i = 0; i < count; ++i)
            target.drawVertexArray({}, vertices.data(), vertices.size(), indices.data(), indices.size(), image);
        return target.finish_recording();
    }
}

TEST(ui_tgui_scene, complete_frame) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    const auto image = texture();
    const auto scene = uploader.upload(provider, recording(image, 2), materials());
    EXPECT_EQ(provider.image_uploads, 1);
    EXPECT_EQ(provider.geometry_uploads, 2);
    ASSERT_EQ(scene.draws().size(), 2u);
    const auto& binding = std::get<CE::Assets::ImageBinding>(scene.draws()[0].parameters.at("pixels"));
    EXPECT_EQ(binding.unit, 3u);
    EXPECT_EQ(binding.image, std::get<CE::Assets::ImageBinding>(scene.draws()[1].parameters.at("pixels")).image);
    CE::RenderAPIs::RenderFrame frame;
    CE::RenderAPIs::RenderFrameWriter writer(frame);
    scene.write(writer);
    ASSERT_EQ(frame.passes().size(), 1u);
    EXPECT_EQ(frame.passes()[0].draws.size(), 2u);
    EXPECT_EQ(frame.passes()[0].draws[1].authored_order, 1u);
    EXPECT_TRUE(frame.passes()[0].draws[0].order_sensitive);
    EXPECT_FLOAT_EQ(frame.passes()[0].projection[0][0], 2.0f / 320);
    EXPECT_FLOAT_EQ(frame.passes()[0].projection[1][1], -2.0f / 240);
}

TEST(ui_tgui_scene, reused_generation) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    auto image = texture();
    const auto selected = materials();
    auto first = uploader.upload(provider, recording(image), selected);
    auto second = uploader.upload(provider, recording(image), selected);
    EXPECT_EQ(provider.image_uploads, 1);
    constexpr std::array<unsigned char, 4> blue{0, 0, 255, 255};
    ASSERT_TRUE(image->loadTextureOnly({1, 1}, blue.data(), true));
    const auto replacement = uploader.upload(provider, recording(image), selected);
    EXPECT_EQ(provider.image_uploads, 2);
    const auto old_pixels =
        std::dynamic_pointer_cast<const MemoryImage>(std::get<CE::Assets::ImageBinding>(first.draws()[0].parameters.at("pixels")).image);
    const auto new_pixels = std::dynamic_pointer_cast<const MemoryImage>(
        std::get<CE::Assets::ImageBinding>(replacement.draws()[0].parameters.at("pixels")).image
    );
    ASSERT_TRUE(old_pixels);
    ASSERT_TRUE(new_pixels);
    EXPECT_EQ(old_pixels->pixels.rgba[0], 255);
    EXPECT_EQ(new_pixels->pixels.rgba[2], 255);
    EXPECT_NE(old_pixels, new_pixels);
    first = Scene{};
    second = Scene{};
    image.reset();
    EXPECT_EQ(old_pixels->pixels.rgba[0], 255);
}

TEST(ui_tgui_scene, retained_frame) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    auto image = texture();
    auto scene = uploader.upload(provider, recording(image), materials());
    CE::RenderAPIs::RenderFrame frame;
    CE::RenderAPIs::RenderFrameWriter writer(frame);
    scene.write(writer);
    const auto image_lifetime = provider.images[0];
    const auto geometry_lifetime = provider.geometry[0];
    image.reset();
    scene = Scene{};
    EXPECT_FALSE(image_lifetime.expired());
    EXPECT_FALSE(geometry_lifetime.expired());
    frame.recycle();
    EXPECT_TRUE(image_lifetime.expired());
    EXPECT_TRUE(geometry_lifetime.expired());
}

TEST(ui_tgui_scene, failed_upload) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    auto current = uploader.upload(provider, recording(), materials());
    const auto previous = current.draws()[0].geometry;
    provider.fail_geometry = provider.geometry_uploads + 2;
    auto completion = std::promise<Scene>{};
    auto future = completion.get_future();
    try {
        completion.set_value(uploader.upload(provider, recording(texture(), 2), materials()));
    } catch (...) {
        completion.set_exception(std::current_exception());
    }
    EXPECT_THROW(adopt_scene(future, current), std::runtime_error);
    ASSERT_EQ(current.draws().size(), 1u);
    EXPECT_EQ(current.draws()[0].geometry, previous);
    ASSERT_EQ(provider.geometry.size(), 2u);
    EXPECT_TRUE(provider.geometry.back().expired());
    EXPECT_TRUE(provider.images.back().expired());
}

TEST(ui_tgui_scene, ready_adoption) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    auto current = uploader.upload(provider, recording(), materials());
    const auto old = current.draws()[0].geometry;
    std::promise<Scene> completion;
    auto future = completion.get_future();
    EXPECT_FALSE(adopt_scene(future, current));
    EXPECT_EQ(current.draws()[0].geometry, old);
    completion.set_value(uploader.upload(provider, recording({}, 2), materials()));
    EXPECT_TRUE(adopt_scene(future, current));
    EXPECT_EQ(current.draws().size(), 2u);
    EXPECT_FALSE(adopt_scene(future, current));
}

TEST(ui_tgui_scene, cancelled_adoption) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    auto current = uploader.upload(provider, recording(), materials());
    const auto old = current.draws()[0].geometry;
    std::future<Scene> future;
    {
        std::promise<Scene> cancelled;
        future = cancelled.get_future();
    }
    EXPECT_THROW(adopt_scene(future, current), std::future_error);
    EXPECT_EQ(current.draws()[0].geometry, old);
}

TEST(ui_tgui_scene, rejected_submission) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    CE::Engine::PlatformDispatcher stopped;
    auto image = texture();
    auto prepared = recording(image);
    std::weak_ptr<const CE::Assets::DecodedImage> pixels = image->snapshot();
    image.reset();
    EXPECT_THROW(
        static_cast<void>(uploader.submit(stopped.submission(), std::move(prepared), materials())), CE::Exceptions::failed_operation
    );
    EXPECT_TRUE(pixels.expired());
    EXPECT_EQ(provider.image_uploads, 0);
    EXPECT_EQ(provider.geometry_uploads, 0);
}

TEST(ui_tgui_scene, owner_domain) {
    MemoryProvider provider;
    MemoryProvider other;
    SceneUploader uploader(provider);
    EXPECT_THROW(static_cast<void>(uploader.upload(other, recording(), materials())), CE::Exceptions::failed_operation);
    std::exception_ptr failure;
    std::thread wrong_owner([&] {
        try {
            static_cast<void>(uploader.upload(provider, recording(), materials()));
        } catch (...) {
            failure = std::current_exception();
        }
    });
    wrong_owner.join();
    ASSERT_TRUE(failure);
    EXPECT_THROW(std::rethrow_exception(failure), CE::Exceptions::failed_operation);
    EXPECT_EQ(provider.geometry_uploads, 0);
}

TEST(ui_tgui_scene, material_contract) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    EXPECT_THROW(static_cast<void>(uploader.upload(provider, recording(), materials(false))), CE::Exceptions::invalid_args);
    auto missing_sampler = materials();
    missing_sampler.image_parameter = "unknown";
    EXPECT_THROW(static_cast<void>(uploader.upload(provider, recording(texture()), missing_sampler)), CE::Exceptions::invalid_args);
    auto no_projection = materials();
    auto definition = no_projection.solid->definition().pipeline->definition();
    definition.parameters.clear();
    no_projection.solid =
        std::make_shared<CE::Assets::Material>(CE::Assets::MaterialDefinition{std::make_shared<MemoryPipeline>(std::move(definition)), {}});
    EXPECT_THROW(static_cast<void>(uploader.upload(provider, recording(), no_projection)), CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(uploader.upload(provider, recording(), Materials{})), CE::Exceptions::invalid_args);
    EXPECT_EQ(provider.image_uploads, 0);
    EXPECT_EQ(provider.geometry_uploads, 0);
}

TEST(ui_tgui_scene, image_contract) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    const auto recorded = recording(texture());
    provider.missing_image = true;
    EXPECT_THROW(static_cast<void>(uploader.upload(provider, recorded, materials())), CE::Exceptions::failed_operation);
    provider.missing_image = false;
    provider.wrong_size = true;
    EXPECT_THROW(static_cast<void>(uploader.upload(provider, recorded, materials())), CE::Exceptions::failed_operation);
    EXPECT_EQ(provider.geometry_uploads, 0);
}

TEST(ui_tgui_scene, empty_scene) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    const auto scene = uploader.upload(provider, RecordedScene{}, Materials{});
    CE::RenderAPIs::RenderFrame frame;
    CE::RenderAPIs::RenderFrameWriter writer(frame);
    scene.write(writer);
    EXPECT_TRUE(frame.passes().empty());
    EXPECT_EQ(provider.image_uploads, 0);
    EXPECT_EQ(provider.geometry_uploads, 0);
}
