#include <ui/tgui/session.h>
#include <ui/tgui/scene.h>
#include <core/controls/input-interface.h>
#include <core/display/display-system-interface.h>
#include <core/engine/engine-context.h>
#include <core/game-framework/abstract-game.h>
#include <core/game-framework/game-runtime.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>

#include <TGUI/Backend/Window/Backend.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Picture.hpp>
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error TGUI runtime checks must not acquire native or graphics SDK headers.
#endif

namespace {
    using namespace CE::UI::TGUI;

    struct RuntimeState {
        const std::thread::id platform = std::this_thread::get_id();
        std::thread::id simulation;
        std::atomic<bool> red_seen{false};
        std::atomic<bool> blue_seen{false};
        CE::RenderAPIs::RenderFrame red_frame;
        CE::RenderAPIs::RenderFrame blue_frame;
        std::vector<std::weak_ptr<const CE::Assets::Image>> images;
        std::vector<std::weak_ptr<const CE::Assets::Geometry2D>> geometry;
        bool input_attached = false;
        bool game_stopped = false;
        bool renderer_stopped = false;
        unsigned presentations = 0;
    };

    class Window final : public CE::iWindow {
        const std::thread::id owner_ = std::this_thread::get_id();
        const std::chrono::steady_clock::time_point deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds{5};

    public:
        CE::ViewPort<int> logical_size() const override {
            EXPECT_EQ(std::this_thread::get_id(), owner_);
            return {320, 240};
        }
        CE::FramebufferSize framebuffer_size() const override {
            EXPECT_EQ(std::this_thread::get_id(), owner_);
            return {640, 480};
        }
        CE::Enum::window_mode mode() const override { return CE::Enum::window_mode::NORMAL; }
        bool should_close() const override { return std::chrono::steady_clock::now() >= deadline_; }
        void resize(int, int) override {}
        void set_mode(CE::Enum::window_mode) override {}
        void hide_cursor(bool) const override {}
    };

    class Display final : public CE::iDisplaySystem {
        std::vector<CE::Monitor> monitors_{{1, 640, 480}};
        Window window_;

    public:
        const std::vector<CE::Monitor>& monitors() const override { return monitors_; }
        int monitor_count() const override { return 1; }
        const CE::Monitor& primary_monitor() const override { return monitors_.front(); }
        CE::iWindow* active_window() const override { return const_cast<Window*>(&window_); }
        std::pair<float, float> content_scale(const CE::Monitor&) const override { return {2, 2}; }
        CE::iWindow* create_window(const CE::Monitor&, CE::Enum::window_mode, int, int) override { return &window_; }
        void activate_window(CE::iWindow&) override {}
    };

    class Input final : public CE::Input::iInputSystem {
        RuntimeState& state_;
        CE::Input::InputBindings bindings_;

    public:
        explicit Input(RuntimeState& state)
        : state_(state) {}
        void initialize(CE::iWindow&) override { state_.input_attached = true; }
        void deinitialize() override {
            discard_captured_input();
            state_.input_attached = false;
        }
        void poll() override {
            EXPECT_EQ(std::this_thread::get_id(), state_.platform);
            begin_input_poll();
            (void)publish_input();
        }
        CE::Input::InputBindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
        bool supports(CE::Input::InputMode) const override { return true; }
        bool supports_focus() const override { return true; }
    };

    struct MemoryImage final : CE::Assets::Image {
        CE::Assets::DecodedImage pixels;

        explicit MemoryImage(CE::Assets::DecodedImage data)
        : pixels(std::move(data)) {}
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

    class Resources final : public CE::Assets::ResourceProvider {
        RuntimeState& state_;

    public:
        explicit Resources(RuntimeState& state)
        : state_(state) {}
        std::shared_ptr<CE::Assets::Image> create_image(const CE::Assets::DecodedImage& data) override {
            EXPECT_EQ(std::this_thread::get_id(), state_.platform);
            auto image = std::make_shared<MemoryImage>(data);
            state_.images.push_back(image);
            return image;
        }
        std::shared_ptr<CE::Assets::Image> create_font_atlas(std::span<const unsigned char>, CE::Assets::PixelSize) override {
            throw std::logic_error("TGUI uploads RGBA font images");
        }
        std::shared_ptr<CE::Assets::Geometry2D> upload_geometry(std::span<const CE::Vertex2D>, CE::Assets::PrimitiveTopology) override {
            throw std::logic_error("TGUI needs colored vertices");
        }
        std::shared_ptr<CE::Assets::Geometry2D>
        upload_geometry(const std::span<const CE::Vertex2DColor> vertices, const CE::Assets::PrimitiveTopology topology) override {
            EXPECT_EQ(std::this_thread::get_id(), state_.platform);
            EXPECT_EQ(topology, CE::Assets::PrimitiveTopology::Triangles);
            auto geometry = std::make_shared<MemoryGeometry>(vertices);
            state_.geometry.push_back(geometry);
            return geometry;
        }
        std::shared_ptr<CE::Assets::Shader> link_program(const std::vector<std::filesystem::path>&) override { return {}; }
    };

    class MemoryPipeline final : public CE::Assets::Pipeline {
    public:
        explicit MemoryPipeline(CE::Assets::PipelineDefinition definition)
        : CE::Assets::Pipeline(std::move(definition)) {}
    };

    Materials materials() {
        using namespace CE::Assets;
        PipelineDefinition definition;
        definition.program_sources = {"controlled-ui"};
        definition.vertex_layout = VertexLayout2D::Position3UV2Color4;
        definition.parameters = {{"projection", ParameterType::Mat4, true, ParameterSemantic::Projection}};
        Materials result;
        result.solid = std::make_shared<Material>(MaterialDefinition{std::make_shared<MemoryPipeline>(definition), {}});
        definition.parameters.push_back({"image", ParameterType::Sampler2D});
        result.textured = std::make_shared<Material>(MaterialDefinition{std::make_shared<MemoryPipeline>(std::move(definition)), {}});
        return result;
    }

    tgui::Texture texture(const bool blue) {
        const std::array<std::uint8_t, 4> pixels =
            blue ? std::array<std::uint8_t, 4>{0, 0, 255, 255} : std::array<std::uint8_t, 4>{255, 0, 0, 255};
        tgui::Texture result;
        result.loadFromPixelData({1, 1}, pixels.data(), {}, {}, true);
        return result;
    }

    std::shared_ptr<const MemoryImage> picture_image(const CE::RenderAPIs::RenderFrame& frame) {
        for (const auto& pass : frame.passes())
            for (const auto& draw : pass.draws) {
                const auto found = draw.parameters.find("image");
                if (found == draw.parameters.end())
                    continue;
                const auto& binding = std::get<CE::Assets::ImageBinding>(found->second);
                auto image = std::dynamic_pointer_cast<const MemoryImage>(binding.image);
                if (image && image->pixels.size.width == 1 && image->pixels.size.height == 1)
                    return image;
            }
        return {};
    }

    // Runtime slots are borrowed. Copy published packets into an owned frame,
    // rather than retaining a pointer to a slot the runtime will recycle.
    void retain_frame(const CE::RenderAPIs::RenderFrame& source, CE::RenderAPIs::RenderFrame& retained) {
        CE::RenderAPIs::RenderFrameWriter writer(retained);
        for (const auto& pass : source.passes()) {
            auto output = writer.begin_pass(pass.projection, pass.view, pass.constraints, pass.parameters);
            for (const auto& draw : pass.draws)
                output.add(draw);
        }
    }

    class Renderer final : public CE::RenderAPIs::iRenderer {
        RuntimeState& state_;

    public:
        explicit Renderer(RuntimeState& state)
        : state_(state) {}
        void initialize() override {}
        void deinitialize() override {
            EXPECT_EQ(std::this_thread::get_id(), state_.platform);
            EXPECT_TRUE(state_.game_stopped);
            EXPECT_FALSE(tgui::isBackendSet());
            state_.renderer_stopped = true;
        }
        void maintain_resources() override {}
        void render(const CE::RenderAPIs::RenderFrame& frame) override {
            EXPECT_EQ(std::this_thread::get_id(), state_.platform);
            const auto image = picture_image(frame);
            if (!image)
                return;
            if (image->pixels.rgba[0] == 255 && !state_.red_seen.load()) {
                retain_frame(frame, state_.red_frame);
                state_.red_seen.store(true);
            } else if (image->pixels.rgba[2] == 255 && !state_.blue_seen.load()) {
                retain_frame(frame, state_.blue_frame);
                state_.blue_seen.store(true);
            }
        }
        void clear() override {}
        void set_viewport(CE::FramebufferSize size) override { EXPECT_EQ(size, (CE::FramebufferSize{640, 480})); }
        void set_depth_test(bool) override {}
        void set_clear_colour(float, float, float, float) override {}
        void set_camera_matrices(const glm::mat4&, const glm::mat4&) override {}
    };

    class Surface final : public CE::RenderAPIs::iPresentationSurface {
        RuntimeState& state_;

    public:
        explicit Surface(RuntimeState& state)
        : state_(state) {}
        void present() override {
            EXPECT_EQ(std::this_thread::get_id(), state_.platform);
            ++state_.presentations;
        }
    };

    class Game final : public CE::GFramework::AbstractGame {
        CE::Engine::EngineContext& engine_;
        RuntimeState& state_;
        SceneUploader uploader_;
        Materials materials_;
        std::unique_ptr<Session> session_;
        tgui::Picture::Ptr picture_;
        Scene current_;
        std::future<Scene> pending_;
        bool blue_submitted_ = false;

    public:
        CE::GFramework::GameRuntime* runtime = nullptr;

        Game(CE::Engine::EngineContext& engine, RuntimeState& state)
        : engine_(engine), state_(state), uploader_(engine.resources()) {}
        void init() override { materials_ = materials(); }
        void deinit() override {
            EXPECT_EQ(std::this_thread::get_id(), state_.platform);
            picture_.reset();
            session_.reset(); // The runtime has joined simulation before this hook.
            current_ = {};
            pending_ = {};
            materials_ = {};
            state_.game_stopped = true;
        }
        void update(const CE::GFramework::TickContext& tick) override {
            EXPECT_EQ(tick.logical_size.width, 320);
            EXPECT_EQ(tick.logical_size.height, 240);
            EXPECT_EQ(tick.framebuffer_size, (CE::FramebufferSize{640, 480}));
            if (!session_) {
                state_.simulation = std::this_thread::get_id();
                session_ = std::make_unique<Session>(engine_.input(), 7);
                session_->set_view(tick.logical_size, tick.framebuffer_size);
                session_->gui().add(tgui::Label::create("Queued UI"));
                picture_ = tgui::Picture::create(texture(false));
                picture_->setPosition({10, 40});
                picture_->setSize({20, 20});
                session_->gui().add(picture_);
                auto recording = session_->record();
                // Mutate the widget before the queued upload sees its old recording.
                picture_->getRenderer()->setTexture(texture(true));
                pending_ = uploader_.submit(engine_.platform_dispatcher().submission(), std::move(recording), materials_);
            } else {
                EXPECT_EQ(std::this_thread::get_id(), state_.simulation);
                (void)adopt_scene(pending_, current_);
                if (state_.red_seen.load() && !blue_submitted_) {
                    pending_ = uploader_.submit(engine_.platform_dispatcher().submission(), session_->record(), materials_);
                    blue_submitted_ = true;
                }
                if (state_.blue_seen.load())
                    runtime->stop();
            }
            session_->update_time(tick.delta_seconds);
            session_->handle_input(tick.input.records(), true);
        }
        void prepare_render_frame(CE::RenderAPIs::RenderFrameWriter& frame) const override {
            EXPECT_EQ(std::this_thread::get_id(), state_.simulation);
            current_.write(frame);
        }
    };

    void run_ui(const CE::GFramework::RunMode mode) {
        RuntimeState state;
        EXPECT_FALSE(tgui::isBackendSet());
        {
            CE::Engine::EngineContext engine(
                std::make_unique<Display>(), std::make_unique<Surface>(state), std::make_unique<Renderer>(state),
                std::make_unique<Resources>(state), std::make_unique<Input>(state)
            );
            Game game(engine, state);
            CE::GFramework::GameRuntime runtime(engine, game, mode);
            game.runtime = &runtime;
            runtime.run();
            EXPECT_TRUE(state.red_seen.load());
            EXPECT_TRUE(state.blue_seen.load());
            EXPECT_TRUE(state.game_stopped);
            EXPECT_TRUE(state.renderer_stopped);
            EXPECT_FALSE(state.input_attached);
            EXPECT_FALSE(tgui::isBackendSet());
            EXPECT_GE(state.presentations, 2u);
            if (mode == CE::GFramework::RunMode::Concurrent)
                EXPECT_NE(state.simulation, state.platform);
            else
                EXPECT_EQ(state.simulation, state.platform);

            const auto red = picture_image(state.red_frame);
            const auto blue = picture_image(state.blue_frame);
            ASSERT_TRUE(red);
            ASSERT_TRUE(blue);
            EXPECT_EQ(red->pixels.rgba, (std::vector<unsigned char>{255, 0, 0, 255}));
            EXPECT_EQ(blue->pixels.rgba, (std::vector<unsigned char>{0, 0, 255, 255}));
            EXPECT_NE(red, blue);
            EXPECT_FLOAT_EQ(state.blue_frame.passes()[0].projection[0][0], 2.0f / 320);
            EXPECT_FLOAT_EQ(state.blue_frame.passes()[0].projection[1][1], -2.0f / 240);
        }
        // Toolkit, game and provider are gone; published packets still own their
        // memory resources until the retaining consumer releases both frames.
        EXPECT_FALSE(state.images.empty());
        EXPECT_FALSE(state.geometry.empty());
        for (const auto& image : state.images)
            EXPECT_FALSE(image.expired());
        for (const auto& geometry : state.geometry)
            EXPECT_FALSE(geometry.expired());
        state.red_frame.recycle();
        state.blue_frame.recycle();
        for (const auto& image : state.images)
            EXPECT_TRUE(image.expired());
        for (const auto& geometry : state.geometry)
            EXPECT_TRUE(geometry.expired());
    }
}

TEST(ui_tgui_runtime, sequential) {
    run_ui(CE::GFramework::RunMode::Sequential);
}

TEST(ui_tgui_runtime, concurrent) {
    run_ui(CE::GFramework::RunMode::Concurrent);
}
