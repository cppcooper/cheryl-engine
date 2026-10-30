#include <gtest/gtest.h>

#include <ui/tgui/scene.h>
#include <internals/exceptions.h>
#include <testing/material-builder.h>

#include <array>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {
    using namespace CE::UI::TGUI;

    struct MemoryImage final : CE::Rendering::Resources::Image {
        CE::Assets::Images::Decoded pixels;

        explicit MemoryImage(CE::Assets::Images::Decoded image)
        : pixels(std::move(image)) {}
        CE::Rendering::Geometry::PixelSize pixel_size() const override { return pixels.size; }
        void bind(std::uint32_t) const override {}
    };

    struct MemoryGeometry final : CE::Rendering::Geometry::Buffer2D {
        std::vector<CE::Rendering::Geometry::Vertex2DColor> vertices;

        explicit MemoryGeometry(const std::span<const CE::Rendering::Geometry::Vertex2DColor> data)
        : vertices(data.begin(), data.end()) {}
        CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override {
            return CE::Rendering::Geometry::Layout2D::Position3UV2Color4;
        }
        CE::Rendering::Geometry::Topology topology() const noexcept override { return CE::Rendering::Geometry::Topology::Triangles; }
        std::size_t vertex_count() const noexcept override { return vertices.size(); }
        void bind() const override {}
        void draw(std::size_t, std::size_t) const override {}
    };

    struct MemorySampler final : CE::Rendering::Sampling::Sampler {
        explicit MemorySampler(const CE::Rendering::Sampling::Options options)
        : CE::Rendering::Sampling::Sampler(options, 1) {}
        void bind(std::uint32_t) const override {}
    };

    struct MemoryProvider final : CE::Rendering::Resources::Provider {
        CE::Assets::Shaders::Builder* builder = nullptr;

        int image_uploads = 0;
        int sampler_uploads = 0;
        int geometry_uploads = 0;
        int fail_geometry = 0;
        bool missing_image = false;
        bool wrong_size = false;
        bool missing_sampler = false;
        bool wrong_sampling = false;
        bool unsupported_sampling = false;
        std::vector<std::weak_ptr<MemoryImage>> images;
        std::vector<std::weak_ptr<const CE::Rendering::Sampling::Sampler>> samplers;
        std::vector<std::weak_ptr<MemoryGeometry>> geometry;

        CE::Assets::Shaders::Builder* shader_asset_builder() noexcept override { return builder; }
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded& data) override {
            ++image_uploads;
            if (missing_image)
                return {};
            auto image = std::make_shared<MemoryImage>(data);
            if (wrong_size)
                ++image->pixels.size.width;
            images.push_back(image);
            return image;
        }
        std::shared_ptr<const CE::Rendering::Sampling::Sampler> create_sampler(const CE::Rendering::Sampling::Options& requested) override {
            ++sampler_uploads;
            if (unsupported_sampling)
                return CE::Rendering::Resources::Provider::create_sampler(requested);
            if (missing_sampler)
                return {};
            auto options = requested;
            if (wrong_sampling)
                options.mipmaps = CE::Rendering::Sampling::MipmapFilter::Linear;
            auto sampler = std::make_shared<MemorySampler>(options);
            samplers.push_back(sampler);
            return sampler;
        }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char>, CE::Rendering::Geometry::PixelSize) override {
            throw std::runtime_error("UI RGBA atlases use create_image");
        }
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D>, CE::Rendering::Geometry::Topology) override {
            throw std::runtime_error("UI requires the colored layout");
        }
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D> upload_geometry(
            const std::span<const CE::Rendering::Geometry::Vertex2DColor> vertices,
            const CE::Rendering::Geometry::Topology topology
        ) override {
            ++geometry_uploads;
            if (fail_geometry == geometry_uploads)
                throw std::runtime_error("injected geometry failure");
            if (topology != CE::Rendering::Geometry::Topology::Triangles)
                throw std::runtime_error("UI requires triangles");
            auto buffer = std::make_shared<MemoryGeometry>(vertices);
            geometry.push_back(buffer);
            return buffer;
        }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override { return {}; }
    };

    class MemoryPipeline final : public CE::Rendering::Resources::Pipeline {
    public:
        explicit MemoryPipeline(CE::Rendering::Pipelines::Definition definition)
        : CE::Rendering::Resources::Pipeline(std::move(definition)) {}
    };

    Materials materials(const bool compatible = true, const std::string& image_parameter = "pixels") {
        CE::Rendering::Pipelines::Definition definition;
        definition.program_sources = {"test-only-ui"};
        definition.vertex_layout =
            compatible ? CE::Rendering::Geometry::Layout2D::Position3UV2Color4 : CE::Rendering::Geometry::Layout2D::Position3UV2;
        definition.parameters = {
            {"projection", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Projection},
            {"view", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::View},
            {"model", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Model}
        };
        Materials result;
        result.solid = std::make_shared<CE::Rendering::Resources::Material>(
            CE::Rendering::Materials::Definition{std::make_shared<MemoryPipeline>(definition), {}}
        );
        definition.parameters.push_back({image_parameter, CE::Rendering::Parameters::Type::Sampler2D});
        result.textured = std::make_shared<CE::Rendering::Resources::Material>(
            CE::Rendering::Materials::Definition{std::make_shared<MemoryPipeline>(std::move(definition)), {}}
        );
        result.image_parameter = image_parameter;
        result.image_unit = 3;
        return result;
    }

    std::shared_ptr<CE::UI::TGUI::Texture> texture(const unsigned char red = 255) {
        auto image = std::make_shared<CE::UI::TGUI::Texture>(32);
        const std::array<unsigned char, 4> pixels{red, 0, 0, 255};
        image->loadTextureOnly({1, 1}, pixels.data(), true);
        return image;
    }

    RecordedScene recording(const std::shared_ptr<CE::UI::TGUI::Texture>& image = {}, const std::size_t count = 1) {
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
    EXPECT_EQ(provider.sampler_uploads, 1);
    EXPECT_EQ(provider.geometry_uploads, 2);
    ASSERT_EQ(scene.draws().size(), 2u);
    const auto& binding = std::get<CE::Rendering::Parameters::ImageBinding>(scene.draws()[0].parameters.at("pixels"));
    EXPECT_EQ(binding.unit, 3u);
    EXPECT_EQ(binding.image, std::get<CE::Rendering::Parameters::ImageBinding>(scene.draws()[1].parameters.at("pixels")).image);
    ASSERT_TRUE(binding.sampler);
    EXPECT_EQ(binding.sampler, std::get<CE::Rendering::Parameters::ImageBinding>(scene.draws()[1].parameters.at("pixels")).sampler);
    EXPECT_EQ(binding.sampler->options().minification, CE::Rendering::Sampling::Filter::Linear);
    EXPECT_EQ(binding.sampler->options().magnification, CE::Rendering::Sampling::Filter::Linear);
    EXPECT_EQ(binding.sampler->options().mipmaps, CE::Rendering::Sampling::MipmapFilter::None);
    EXPECT_EQ(binding.sampler->options().wrap_u, CE::Rendering::Sampling::Wrap::ClampToEdge);
    EXPECT_EQ(binding.sampler->options().wrap_v, CE::Rendering::Sampling::Wrap::ClampToEdge);
    EXPECT_EQ(binding.sampler->options().anisotropy, CE::Rendering::Sampling::Anisotropy::Disabled);
    CE::Rendering::Frame frame;
    CE::Rendering::FrameWriter writer(frame);
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
    const auto old_pixels = std::dynamic_pointer_cast<const MemoryImage>(
        std::get<CE::Rendering::Parameters::ImageBinding>(first.draws()[0].parameters.at("pixels")).image
    );
    const auto new_pixels = std::dynamic_pointer_cast<const MemoryImage>(
        std::get<CE::Rendering::Parameters::ImageBinding>(replacement.draws()[0].parameters.at("pixels")).image
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
    CE::Rendering::Frame frame;
    CE::Rendering::FrameWriter writer(frame);
    scene.write(writer);
    const auto image_lifetime = provider.images[0];
    const auto geometry_lifetime = provider.geometry[0];
    const auto sampler_lifetime = provider.samplers[0];
    image.reset();
    scene = Scene{};
    EXPECT_FALSE(image_lifetime.expired());
    EXPECT_FALSE(geometry_lifetime.expired());
    EXPECT_FALSE(sampler_lifetime.expired());
    frame.recycle();
    EXPECT_TRUE(image_lifetime.expired());
    EXPECT_TRUE(geometry_lifetime.expired());
    EXPECT_TRUE(sampler_lifetime.expired());
}

TEST(ui_tgui_scene, sampling_isolation) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    const auto image = texture();
    const auto selected = materials();
    const auto smooth_recording = recording(image);
    image->setSmooth(false);
    const auto nearest_recording = recording(image);
    const auto nearest = uploader.upload(provider, nearest_recording, selected);
    const auto smooth = uploader.upload(provider, smooth_recording, selected);
    const auto reused = uploader.upload(provider, nearest_recording, selected);
    EXPECT_EQ(provider.image_uploads, 1);
    EXPECT_EQ(provider.sampler_uploads, 2);
    const auto& nearest_binding = std::get<CE::Rendering::Parameters::ImageBinding>(nearest.draws()[0].parameters.at("pixels"));
    const auto& smooth_binding = std::get<CE::Rendering::Parameters::ImageBinding>(smooth.draws()[0].parameters.at("pixels"));
    const auto& reused_binding = std::get<CE::Rendering::Parameters::ImageBinding>(reused.draws()[0].parameters.at("pixels"));
    EXPECT_EQ(nearest_binding.image, smooth_binding.image);
    EXPECT_EQ(nearest_binding.image, reused_binding.image);
    ASSERT_TRUE(nearest_binding.sampler);
    ASSERT_TRUE(smooth_binding.sampler);
    EXPECT_NE(nearest_binding.sampler, smooth_binding.sampler);
    EXPECT_EQ(nearest_binding.sampler, reused_binding.sampler);
    EXPECT_EQ(nearest_binding.sampler->options().minification, CE::Rendering::Sampling::Filter::Nearest);
    EXPECT_EQ(nearest_binding.sampler->options().magnification, CE::Rendering::Sampling::Filter::Nearest);
    EXPECT_EQ(nearest_binding.sampler->options().mipmaps, CE::Rendering::Sampling::MipmapFilter::None);
    EXPECT_FLOAT_EQ(nearest_binding.sampler->effective_anisotropy(), 1);
    EXPECT_EQ(smooth_binding.sampler->options().minification, CE::Rendering::Sampling::Filter::Linear);
    EXPECT_EQ(smooth_binding.sampler->options().magnification, CE::Rendering::Sampling::Filter::Linear);
}

TEST(ui_tgui_scene, sampling_failure) {
    for (const int failure : {0, 1, 2}) {
        SCOPED_TRACE(failure);
        MemoryProvider provider;
        SceneUploader uploader(provider);
        const auto image = texture();
        auto current = uploader.upload(provider, recording(image), materials());
        const auto previous = current.draws()[0].geometry;
        const auto old_sampler = std::get<CE::Rendering::Parameters::ImageBinding>(current.draws()[0].parameters.at("pixels")).sampler;
        image->setSmooth(false);
        provider.unsupported_sampling = failure == 0;
        provider.missing_sampler = failure == 1;
        provider.wrong_sampling = failure == 2;
        std::promise<Scene> completion;
        auto future = completion.get_future();
        try {
            completion.set_value(uploader.upload(provider, recording(image), materials()));
        } catch (...) {
            completion.set_exception(std::current_exception());
        }
        EXPECT_THROW(adopt_scene(future, current), CE::Exceptions::failed_operation);
        EXPECT_EQ(current.draws()[0].geometry, previous);
        EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(current.draws()[0].parameters.at("pixels")).sampler, old_sampler);
        EXPECT_EQ(provider.image_uploads, 1);
        EXPECT_EQ(provider.geometry_uploads, 1);
        provider.unsupported_sampling = provider.missing_sampler = provider.wrong_sampling = false;
        const auto retried = uploader.upload(provider, recording(image), materials());
        EXPECT_EQ(provider.image_uploads, 1);
        EXPECT_EQ(provider.sampler_uploads, 3);
        EXPECT_EQ(
            std::get<CE::Rendering::Parameters::ImageBinding>(retried.draws()[0].parameters.at("pixels")).sampler->options().magnification,
            CE::Rendering::Sampling::Filter::Nearest
        );
    }
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
    CE::Runtime::PlatformDispatcher stopped;
    auto image = texture();
    auto prepared = recording(image);
    std::weak_ptr<const CE::Assets::Images::Decoded> pixels = image->snapshot();
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
    no_projection.solid = std::make_shared<CE::Rendering::Resources::Material>(
        CE::Rendering::Materials::Definition{std::make_shared<MemoryPipeline>(std::move(definition)), {}}
    );
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
    CE::Rendering::Frame frame;
    CE::Rendering::FrameWriter writer(frame);
    scene.write(writer);
    EXPECT_TRUE(frame.passes().empty());
    EXPECT_EQ(provider.image_uploads, 0);
    EXPECT_EQ(provider.geometry_uploads, 0);
}

TEST(ui_tgui_scene, defaults) {
    const auto supplied = materials(true, "image");
    CE::Testing::MaterialBuilder builder;
    builder.values = {
        {CE::Rendering::Materials::Builtin2D::SolidColor, supplied.solid},
        {CE::Rendering::Materials::Builtin2D::TexturedColor, supplied.textured}
    };
    MemoryProvider provider;
    provider.builder = &builder;
    SceneUploader uploader(provider);
    EXPECT_TRUE(builder.requests.empty());
    const auto solid = uploader.upload(provider, recording());
    const auto textured = uploader.upload(provider, recording(texture()));
    ASSERT_EQ(builder.requests.size(), 2u);
    EXPECT_EQ(
        builder.requests[0], (std::pair{CE::Rendering::Materials::Builtin2D::SolidColor, CE::Rendering::Pipelines::Blend::StraightAlpha})
    );
    EXPECT_EQ(
        builder.requests[1], (std::pair{CE::Rendering::Materials::Builtin2D::TexturedColor, CE::Rendering::Pipelines::Blend::StraightAlpha})
    );
    ASSERT_EQ(solid.draws().size(), 1u);
    ASSERT_EQ(textured.draws().size(), 1u);
    EXPECT_EQ(solid.draws()[0].material, supplied.solid);
    EXPECT_EQ(textured.draws()[0].material, supplied.textured);
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(textured.draws()[0].parameters.at("image")).unit, 0u);
    const auto custom = materials();
    const auto overridden = uploader.upload(provider, recording(texture()), custom);
    EXPECT_EQ(overridden.draws()[0].material, custom.textured);
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(overridden.draws()[0].parameters.at("pixels")).unit, 3u);
    const auto restored = uploader.upload(provider, recording());
    EXPECT_EQ(restored.draws()[0].material, supplied.solid);
    EXPECT_EQ(builder.requests.size(), 2u);
}

TEST(ui_tgui_scene, unsupported_defaults) {
    MemoryProvider provider;
    SceneUploader uploader(provider);
    EXPECT_THROW(static_cast<void>(uploader.upload(provider, recording())), CE::Exceptions::failed_operation);
    EXPECT_TRUE(uploader.upload(provider, RecordedScene{}).draws().empty());
    EXPECT_NO_THROW(static_cast<void>(uploader.upload(provider, recording(), materials())));
}

TEST(ui_tgui_scene, failed_defaults) {
    for (const bool missing : {false, true}) {
        SCOPED_TRACE(missing);
        const auto supplied = materials(true, "image");
        CE::Testing::MaterialBuilder builder;
        builder.values = {
            {CE::Rendering::Materials::Builtin2D::SolidColor, supplied.solid},
            {CE::Rendering::Materials::Builtin2D::TexturedColor, supplied.textured}
        };
        builder.missing = missing;
        builder.fail_request = missing ? 0 : 2;
        MemoryProvider provider;
        provider.builder = &builder;
        SceneUploader uploader(provider);
        if (missing)
            EXPECT_THROW(static_cast<void>(uploader.upload(provider, recording())), CE::Exceptions::failed_operation);
        else
            EXPECT_THROW(static_cast<void>(uploader.upload(provider, recording())), std::runtime_error);
        EXPECT_EQ(provider.geometry_uploads, 0);
        builder.fail_request = 0;
        builder.missing = false;
        const auto scene = uploader.upload(provider, recording());
        EXPECT_EQ(scene.draws().size(), 1u);
        EXPECT_EQ(builder.requests.size(), 4u);
    }
}

TEST(ui_tgui_scene, default_owner) {
    CE::Testing::MaterialBuilder builder;
    MemoryProvider provider;
    provider.builder = &builder;
    MemoryProvider other;
    other.builder = &builder;
    SceneUploader uploader(provider);
    const auto recorded = recording();
    EXPECT_THROW(static_cast<void>(uploader.upload(other, recorded)), CE::Exceptions::failed_operation);
    std::exception_ptr error;
    std::thread foreign([&] {
        try {
            static_cast<void>(uploader.upload(provider, recorded));
        } catch (...) {
            error = std::current_exception();
        }
    });
    foreign.join();
    ASSERT_TRUE(error);
    EXPECT_THROW(std::rethrow_exception(error), CE::Exceptions::failed_operation);
    EXPECT_TRUE(builder.requests.empty());
}
