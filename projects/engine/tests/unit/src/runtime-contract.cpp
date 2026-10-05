#include <core/engine/engine-context.h>
#include <core/game-framework/abstract-game.h>
#include <core/game-framework/game-runtime.h>
#include <core/controls/input-interface.h>
#include <core/display/display-system-interface.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <assets/resources/resource-provider.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error Engine runtime contracts must compile without native or graphics SDK headers.
#endif

namespace {
    struct Observations {
        CE::iWindow* attached = nullptr;
        CE::FramebufferSize viewport{};
        int updates = 0;
        int frames = 0;
        int renders = 0;
        int presentations = 0;
        int initializations = 0;
        int shutdowns = 0;
        bool pressed = false;
    };

    class ContractWindow final : public CE::iWindow {
        const std::thread::id owner_ = std::this_thread::get_id();

    public:
        CE::ViewPort<int> logical_size() const override {
            EXPECT_EQ(std::this_thread::get_id(), owner_);
            return {16, 12};
        }
        CE::FramebufferSize framebuffer_size() const override {
            EXPECT_EQ(std::this_thread::get_id(), owner_);
            return {32, 24};
        }
        CE::Enum::window_mode mode() const override { return CE::Enum::window_mode::NORMAL; }
        bool should_close() const override { return false; }
        void resize(int, int) override {}
        void set_mode(CE::Enum::window_mode) override {}
        void hide_cursor(bool) const override {}
    };

    class ContractDisplay final : public CE::iDisplaySystem {
        std::vector<CE::Monitor> monitors_{{1, 32, 24}};
        ContractWindow window_;

    public:
        const std::vector<CE::Monitor>& monitors() const override { return monitors_; }
        int monitor_count() const override { return 1; }
        const CE::Monitor& primary_monitor() const override { return monitors_.front(); }
        CE::iWindow* active_window() const override { return const_cast<ContractWindow*>(&window_); }
        std::pair<float, float> content_scale(const CE::Monitor&) const override { return {1, 1}; }
        CE::iWindow* create_window(const CE::Monitor&, CE::Enum::window_mode, int, int) override { return &window_; }
        void activate_window(CE::iWindow&) override {}
    };

    class ContractInput final : public CE::Input::iInputSystem {
        Observations& observations_;
        CE::Input::InputBindings bindings_;

    public:
        explicit ContractInput(Observations& observations)
        : observations_(observations) {}
        void initialize(CE::iWindow& window) override { observations_.attached = &window; }
        void deinitialize() override { observations_.attached = nullptr; }
        void poll() override {
            bindings_.on_button({keyboard_id(), 65}, true);
            (void)bindings_.publish_actions();
        }
        CE::Input::InputBindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
    };

    class ContractSurface final : public CE::RenderAPIs::iPresentationSurface {
        Observations& observations_;

    public:
        explicit ContractSurface(Observations& observations)
        : observations_(observations) {}
        void present() override { ++observations_.presentations; }
    };

    class ContractRenderer final : public CE::RenderAPIs::iRenderer {
        Observations& observations_;

    public:
        explicit ContractRenderer(Observations& observations)
        : observations_(observations) {}
        void initialize() override { ++observations_.initializations; }
        void deinitialize() override { ++observations_.shutdowns; }
        void maintain_resources() override {}
        void render(const CE::RenderAPIs::RenderFrame& frame) override {
            EXPECT_EQ(frame.passes().size(), 1u);
            ++observations_.renders;
        }
        void clear() override {}
        void set_viewport(CE::FramebufferSize size) override { observations_.viewport = size; }
        void set_depth_test(bool) override {}
        void set_clear_colour(float, float, float, float) override {}
        void set_camera_matrices(const glm::mat4&, const glm::mat4&) override {}
    };

    class ContractResources final : public CE::Assets::ResourceProvider {
    public:
        std::shared_ptr<CE::Assets::Image> create_image(const CE::Assets::DecodedImage&) override {
            throw std::logic_error("This contract scenario does not upload images");
        }
        std::shared_ptr<CE::Assets::Image> create_font_atlas(std::span<const unsigned char>, CE::Assets::PixelSize) override {
            throw std::logic_error("This contract scenario does not upload fonts");
        }
        std::shared_ptr<CE::Assets::Geometry2D> upload_geometry(std::span<const CE::Vertex2D>, CE::Assets::PrimitiveTopology) override {
            throw std::logic_error("This contract scenario does not upload geometry");
        }
        std::shared_ptr<CE::Assets::Shader> link_program(const std::vector<std::filesystem::path>&) override {
            throw std::logic_error("This contract scenario does not link shaders");
        }
    };

    class ContractGame final : public CE::GFramework::AbstractGame {
        CE::Engine::EngineContext& engine_;
        Observations& observations_;

    public:
        CE::GFramework::GameRuntime* runtime = nullptr;

        ContractGame(CE::Engine::EngineContext& engine, Observations& observations)
        : engine_(engine), observations_(observations) {}
        void init() override { (void)engine_.input().bindings().bind_button({1, 65}, CE::Input::ActionId{1}); }
        void deinit() override {}
        void update(const CE::GFramework::TickContext& tick) override {
            observations_.pressed = tick.input.button(CE::Input::ActionId{1}).pressed();
            EXPECT_EQ(tick.framebuffer_size, (CE::FramebufferSize{32, 24}));
            EXPECT_EQ(tick.logical_size.width, 16);
            EXPECT_EQ(tick.logical_size.height, 12);
            ++observations_.updates;
            runtime->stop();
        }
        void prepare_render_frame(CE::RenderAPIs::RenderFrameWriter& frame) const override {
            (void)frame.begin_pass(glm::mat4{1}, glm::mat4{1});
            ++observations_.frames;
        }
    };
}

TEST(runtime_contract, frame) {
    Observations observations;
    CE::Engine::EngineContext engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    ContractGame game(engine, observations);
    CE::GFramework::GameRuntime runtime(engine, game);
    game.runtime = &runtime;
    runtime.run();
    EXPECT_TRUE(observations.pressed);
    EXPECT_EQ(observations.updates, 1);
    EXPECT_EQ(observations.frames, 1);
    EXPECT_EQ(observations.renders, 1);
    EXPECT_EQ(observations.presentations, 1);
    EXPECT_EQ(observations.initializations, 1);
    EXPECT_EQ(observations.shutdowns, 1);
    EXPECT_EQ(observations.viewport, (CE::FramebufferSize{32, 24}));
    EXPECT_EQ(observations.attached, nullptr);
    const auto diagnostics = runtime.diagnostics();
    EXPECT_EQ(diagnostics.updates, 1u);
    EXPECT_EQ(diagnostics.published, 1u);
    EXPECT_EQ(diagnostics.rendered, 1u);
    EXPECT_THROW(runtime.run(), CE::Exceptions::failed_operation);
}

TEST(runtime_contract, concurrent_window_snapshot) {
    Observations observations;
    CE::Engine::EngineContext engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    ContractGame game(engine, observations);
    CE::GFramework::GameRuntime runtime(engine, game, CE::GFramework::RunMode::Concurrent);
    game.runtime = &runtime;
    runtime.run();
    EXPECT_EQ(observations.updates, 1);
    EXPECT_EQ(observations.attached, nullptr);
}

TEST(runtime_contract, missing_display) {
    Observations observations;
    EXPECT_THROW(
        CE::Engine::EngineContext(
            nullptr, std::make_unique<ContractSurface>(observations), std::make_unique<ContractRenderer>(observations),
            std::make_unique<ContractResources>(), std::make_unique<ContractInput>(observations)
        ),
        CE::Exceptions::invalid_args
    );
}
