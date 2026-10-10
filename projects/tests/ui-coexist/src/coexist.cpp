#include <ui/tgui/session.h>
#include <ui/tgui/scene.h>
#include <ui/rmlui/session.h>
#include <ui/rmlui/scene.h>

#include <TGUI/Backend/Window/Backend.hpp>
#include <TGUI/Widgets/EditBox.hpp>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <gtest/gtest.h>

#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error UI coexistence uses Engine contracts without native or graphics SDKs.
#endif

namespace {
    class Input final : public CE::Input::iInputSystem {
        CE::Input::InputBindings bindings_;

    public:
        void initialize(CE::iWindow&) override {}
        void poll() override {}
        void deinitialize() override {}
        CE::Input::InputBindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
        bool supports(CE::Input::InputMode) const override { return true; }
        bool supports_focus() const override { return true; }
        std::shared_ptr<const CE::Input::PollSnapshot> text(const char32_t character) {
            begin_input_poll();
            capture_buffer().record(1, CE::Input::DeviceKind::Keyboard, CE::Input::TextEvent{character});
            return publish_input();
        }
    };

    struct Image final : CE::Assets::Image {
        CE::Assets::DecodedImage pixels;

        explicit Image(CE::Assets::DecodedImage pixels)
        : pixels(std::move(pixels)) {}
        CE::Assets::PixelSize pixel_size() const override { return pixels.size; }
        void bind(std::uint32_t) const override {}
    };
    struct Geometry final : CE::Assets::Geometry2D {
        std::vector<CE::Vertex2DColor> vertices;

        explicit Geometry(const std::span<const CE::Vertex2DColor> vertices)
        : vertices(vertices.begin(), vertices.end()) {}
        CE::Assets::VertexLayout2D vertex_layout() const noexcept override { return CE::Assets::VertexLayout2D::Position3UV2Color4; }
        CE::Assets::PrimitiveTopology topology() const noexcept override { return CE::Assets::PrimitiveTopology::Triangles; }
        std::size_t vertex_count() const noexcept override { return vertices.size(); }
        void bind() const override {}
        void draw(std::size_t, std::size_t) const override {}
    };
    struct MemorySampler final : CE::Assets::Sampler {
        explicit MemorySampler(const CE::Assets::SamplerOptions options)
        : Sampler(options, 1) {}
        void bind(std::uint32_t) const override {}
    };
    class Provider final : public CE::Assets::ResourceProvider {
    public:
        std::vector<std::weak_ptr<Image>> images;
        std::vector<std::weak_ptr<const CE::Assets::Sampler>> samplers;
        std::vector<std::weak_ptr<Geometry>> geometry;

        std::shared_ptr<CE::Assets::Image> create_image(const CE::Assets::DecodedImage& pixels) override {
            auto image = std::make_shared<Image>(pixels);
            images.push_back(image);
            return image;
        }
        std::shared_ptr<const CE::Assets::Sampler> create_sampler(const CE::Assets::SamplerOptions& options) override {
            auto sampler = std::make_shared<MemorySampler>(options);
            samplers.push_back(sampler);
            return sampler;
        }
        std::shared_ptr<CE::Assets::Image> create_font_atlas(std::span<const unsigned char>, CE::Assets::PixelSize) override {
            throw std::logic_error("Both toolkits publish owned RGBA atlases");
        }
        std::shared_ptr<CE::Assets::Geometry2D> upload_geometry(std::span<const CE::Vertex2D>, CE::Assets::PrimitiveTopology) override {
            throw std::logic_error("Both toolkits use colored vertices");
        }
        std::shared_ptr<CE::Assets::Geometry2D>
        upload_geometry(std::span<const CE::Vertex2DColor> vertices, CE::Assets::PrimitiveTopology) override {
            auto buffer = std::make_shared<Geometry>(vertices);
            geometry.push_back(buffer);
            return buffer;
        }
        std::shared_ptr<CE::Assets::Shader> link_program(const std::vector<std::filesystem::path>&) override { return {}; }
    };
    class Pipeline final : public CE::Assets::Pipeline {
    public:
        explicit Pipeline(CE::Assets::PipelineDefinition definition)
        : CE::Assets::Pipeline(std::move(definition)) {}
    };
    template <typename Materials> Materials materials(const CE::Assets::BlendMode blend) {
        using namespace CE::Assets;
        PipelineDefinition definition;
        definition.program_sources = {"coexist-ui"};
        definition.vertex_layout = VertexLayout2D::Position3UV2Color4;
        definition.state.blend = blend;
        definition.parameters = {{"projection", ParameterType::Mat4, true, ParameterSemantic::Projection}};
        Materials result;
        result.solid = std::make_shared<Material>(MaterialDefinition{std::make_shared<::Pipeline>(definition), {}});
        definition.parameters.push_back({"image", ParameterType::Sampler2D});
        result.textured = std::make_shared<Material>(MaterialDefinition{std::make_shared<::Pipeline>(std::move(definition)), {}});
        return result;
    }
}

TEST(ui_coexist, focus_lifetime) {
    Input input;
    CE::RenderAPIs::RenderFrame frame;
    CE::UI::TGUI::RecordedScene retained_tgui;
    CE::UI::RmlUi::RecordedScene retained_rmlui;
    std::vector<std::weak_ptr<Image>> images;
    std::vector<std::weak_ptr<const CE::Assets::Sampler>> samplers;
    std::vector<std::weak_ptr<Geometry>> geometry;
    {
        Provider provider;
        auto tgui_session = std::make_unique<CE::UI::TGUI::Session>(input, 11);
        tgui_session->set_view({320, 240}, {640, 480});
        auto tgui_field = tgui::EditBox::create();
        tgui_field->setSize({180, 30});
        tgui_session->gui().add(tgui_field);

        auto rml_session = std::make_unique<CE::UI::RmlUi::Session>(input, 22);
        ASSERT_TRUE(rml_session->load_font(std::filesystem::path{CHERYL_RMLUI_TEST_FONT}, "coexist"));
        rml_session->set_view({320, 240}, {640, 480});
        auto* document = rml_session->context().LoadDocumentFromMemory(
            "<rml><head><style>body { font-family: coexist; font-size:18px; }"
            "input { width:180px; height:30px; background-color:#eee; }</style></head>"
            "<body><input id='field' type='text' /></body></rml>"
        );
        ASSERT_TRUE(document);
        document->Show();
        rml_session->update_time(0);
        auto* rml_field = dynamic_cast<Rml::ElementFormControlInput*>(document->GetElementById("field"));
        ASSERT_TRUE(rml_field);
        const auto deliver = [&](const CE::Input::PollSnapshot& poll) {
            tgui_session->handle_input(poll.records, false);
            rml_session->handle_input(poll.records, false);
        };
        tgui_session->request_keyboard_focus();
        tgui_field->setFocused(true);
        const auto first = input.text(U'a');
        deliver(*first);
        EXPECT_EQ(tgui_field->getText(), "a");
        EXPECT_EQ(rml_field->GetValue(), "");

        const auto latched = input.text(U'b');
        rml_session->request_keyboard_focus();
        ASSERT_TRUE(rml_field->Focus());
        deliver(*latched); // This poll still belongs to TGUI, before preemption.
        EXPECT_EQ(tgui_field->getText(), "ab");
        EXPECT_EQ(rml_field->GetValue(), "");
        EXPECT_TRUE(rml_session->owns_keyboard_focus());
        EXPECT_FALSE(tgui_session->owns_keyboard_focus());
        const auto second = input.text(U'c');
        deliver(*second);
        EXPECT_EQ(tgui_field->getText(), "ab");
        EXPECT_EQ(rml_field->GetValue(), "c");
        EXPECT_EQ(first->records[0].target, 11u);
        EXPECT_EQ(latched->records[0].target, 11u);
        EXPECT_EQ(second->records[0].target, 22u);
        tgui_session->release_keyboard_focus();
        EXPECT_TRUE(rml_session->owns_keyboard_focus());

        retained_tgui = tgui_session->record();
        retained_rmlui = rml_session->record();
        CE::UI::TGUI::SceneUploader tgui_uploader(provider);
        CE::UI::RmlUi::SceneUploader rml_uploader(provider);
        const auto tgui_scene =
            tgui_uploader.upload(provider, retained_tgui, materials<CE::UI::TGUI::Materials>(CE::Assets::BlendMode::StraightAlpha));
        const auto rml_scene =
            rml_uploader.upload(provider, retained_rmlui, materials<CE::UI::RmlUi::Materials>(CE::Assets::BlendMode::PremultipliedAlpha));
        CE::RenderAPIs::RenderFrameWriter writer(frame);
        tgui_scene.write(writer);
        rml_scene.write(writer);
        ASSERT_EQ(frame.passes().size(), 2u);
        EXPECT_EQ(frame.passes()[0].constraints.blend, CE::Assets::BlendMode::StraightAlpha);
        EXPECT_EQ(frame.passes()[1].constraints.blend, CE::Assets::BlendMode::PremultipliedAlpha);
        images = provider.images;
        samplers = provider.samplers;
        geometry = provider.geometry;
        for (std::size_t i = 0; i < frame.passes().size(); ++i) {
            for (const auto& draw : frame.passes()[i].draws) {
                if (const auto found = draw.parameters.find("image"); found != draw.parameters.end()) {
                    const auto& binding = std::get<CE::Assets::ImageBinding>(found->second);
                    EXPECT_EQ(static_cast<bool>(binding.sampler), i == 0);
                }
            }
        }

        tgui_field.reset();
        tgui_session.reset();
        EXPECT_FALSE(tgui::isBackendSet());
        EXPECT_NE(Rml::GetSystemInterface(), nullptr);
        EXPECT_TRUE(rml_session->owns_keyboard_focus());
        document = nullptr;
        rml_field = nullptr;
        rml_session.reset();
        EXPECT_EQ(Rml::GetSystemInterface(), nullptr);
    }
    ASSERT_FALSE(images.empty());
    ASSERT_FALSE(geometry.empty());
    ASSERT_FALSE(samplers.empty());
    EXPECT_FALSE(retained_tgui.draws().empty());
    EXPECT_FALSE(retained_rmlui.draws().empty());
    for (const auto& image : images)
        EXPECT_FALSE(image.expired());
    for (const auto& buffer : geometry)
        EXPECT_FALSE(buffer.expired());
    for (const auto& sampler : samplers)
        EXPECT_FALSE(sampler.expired());
    frame.recycle();
    for (const auto& image : images)
        EXPECT_TRUE(image.expired());
    for (const auto& buffer : geometry)
        EXPECT_TRUE(buffer.expired());
    for (const auto& sampler : samplers)
        EXPECT_TRUE(sampler.expired());
}
