#include <gtest/gtest.h>

#include <assets/types/2d/graphic.h>
#include <assets/resources/resource-provider.h>
#include <core/rendering/draw-info.h>
#include <core/engine/engine-context.h>
#include <core/game-framework/abstract-game.h>
#include <core/game-framework/game-runtime.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <core/display/display-system-interface.h>
#include <core/display/window-interface.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error The alternative runtime adapter must not include OpenGL or GLFW headers.
#endif

namespace {
    constexpr CE::Input::DeviceButtonId test_button = 65;

    /** Holds mutable window state for a runtime test without a native window. */
    class MemoryWindow final : public CE::iWindow {
    public:
        [[nodiscard]] CE::ViewPort<int> logical_size() const override { return {size_.width, size_.height}; }
        [[nodiscard]] CE::FramebufferSize framebuffer_size() const override { return size_; }
        [[nodiscard]] CE::Enum::window_mode mode() const override { return mode_; }
        [[nodiscard]] bool should_close() const override { return closed_; }
        void resize(int width, int height) override { size_ = {width, height}; }
        void set_mode(CE::Enum::window_mode mode) override { mode_ = mode; }
        void hide_cursor(bool hide) const override { cursor_hidden_ = hide; }
        void request_close() { closed_ = true; }
        [[nodiscard]] bool cursor_hidden() const { return cursor_hidden_; }

    private:
        CE::FramebufferSize size_{320, 240};
        CE::Enum::window_mode mode_ = CE::Enum::window_mode::NORMAL;
        bool closed_ = false;
        mutable bool cursor_hidden_ = false;
    };

    /** Creates and activates MemoryWindow through the display interface. */
    class MemoryDisplay final : public CE::iDisplaySystem {
    public:
        [[nodiscard]] const std::vector<CE::Monitor>& monitors() const override { return monitors_; }
        [[nodiscard]] int monitor_count() const override { return static_cast<int>(monitors_.size()); }
        [[nodiscard]] const CE::Monitor& primary_monitor() const override { return monitors_.front(); }
        [[nodiscard]] CE::iWindow* active_window() const override { return active_; }
        [[nodiscard]] std::pair<float, float> content_scale(const CE::Monitor&) const override { return {1, 1}; }
        CE::iWindow* create_window(const CE::Monitor&, CE::Enum::window_mode mode, int width, int height) override {
            window_ = std::make_unique<MemoryWindow>();
            window_->resize(width, height);
            window_->set_mode(mode);
            return window_.get();
        }
        void activate_window(CE::iWindow& window) override { active_ = &window; }

        [[nodiscard]] MemoryWindow& window() { return *window_; }

    private:
        std::vector<CE::Monitor> monitors_{{1, 320, 240}};
        std::unique_ptr<MemoryWindow> window_;
        CE::iWindow* active_ = nullptr;
    };

    /** Emits one button transition per poll and records its attached window. */
    class MemoryInput final : public CE::Input::iInputSystem {
        CE::iWindow* window_ = nullptr;
        CE::Input::InputBindings bindings_;

    public:
        std::function<void()> on_poll;
        void initialize(CE::iWindow& window) override { window_ = &window; }
        void poll() override {
            if (on_poll) on_poll();
            bindings_.on_button({keyboard_id(), test_button}, true);
            (void)bindings_.publish_actions();
        }
        void deinitialize() override { window_ = nullptr; }
        [[nodiscard]] CE::Input::InputBindings& bindings() override { return bindings_; }
        [[nodiscard]] CE::Input::DeviceId keyboard_id() const override { return 1; }
        [[nodiscard]] CE::Input::DeviceId mouse_id() const override { return 2; }
        [[nodiscard]] CE::Input::DeviceId gamepad_id() const override { return 3; }
        [[nodiscard]] CE::iWindow* attached_window() const { return window_; }
    };

    /** Records which thread receives the semantic press and owns frame preparation. */
    class OneTickGame final : public CE::GFramework::AbstractGame {
        static constexpr CE::Input::ActionId action{17};
        CE::Input::iInputSystem& input_;

    public:
        std::function<void()> on_tick;
        int updates = 0;
        mutable int draws = 0;
        std::atomic<bool> pressed{false};
        std::thread::id update_thread;
        CE::FramebufferSize size{};

        explicit OneTickGame(CE::Input::iInputSystem& input) : input_(input) {}

        void init() override { (void)input_.bindings().bind_button({input_.keyboard_id(), test_button}, action); }
        void deinit() override { input_.bindings().clear(); }
        void update(const CE::GFramework::TickContext& tick) override {
            ++updates;
            update_thread = std::this_thread::get_id();
            size = tick.framebuffer_size;
            if (tick.input.button(action).pressed()) pressed.store(true);
            if (pressed.load() && on_tick)
                on_tick();
        }
        void prepare_render_frame(CE::RenderAPIs::RenderFrameWriter& frame) const override {
            ++draws;
            glm::mat4 view{1.0f};
            view[3][0] = pressed.load() ? 1.0f : 0.0f;
            (void)frame.begin_pass(glm::mat4{1.0f}, view);
        }
    };

    class MemoryImage final : public CE::Assets::Image {
    public:
        explicit MemoryImage(CE::Assets::PixelSize size) : size_(size) {}
        [[nodiscard]] CE::Assets::PixelSize pixel_size() const override { return size_; }

    private:
        CE::Assets::PixelSize size_;
    };

    /** Records image binding and draw ranges instead of issuing GPU commands. */
    class MemoryGeometry final : public CE::Assets::Geometry2D {
    public:
        void bind(const CE::Assets::Image& image) const override { bound_size = image.pixel_size(); }
        void draw(std::size_t first, std::size_t count) const override {
            first_vertex = first;
            drawn_vertices = count;
        }

        mutable CE::Assets::PixelSize bound_size{};
        mutable std::size_t first_vertex = 0;
        mutable std::size_t drawn_vertices = 0;
    };

    /** Records shader uses and camera matrices passed during drawing. */
    class MemoryShader final : public CE::Assets::Shader {
    public:
        void use() override { ++uses; }
        void set_uniform_value(const char*, float) override {}
        void set_uniform_value(const char*, int) override {}
        void set_uniform_value(const char*, unsigned int) override {}
        void set_uniform_value(const char*, bool) override {}
        void set_uniform_matrix(const char* name, const glm::mat4& value) override {
            if (std::string_view(name) == "projectionMatrix")
                projection = value;
            if (std::string_view(name) == "viewMatrix")
                view = value;
        }

        int uses = 0;
        glm::mat4 projection{0.0f};
        glm::mat4 view{0.0f};
    };

    /** Supplies in-memory assets and records the geometry uploaded by asset managers. */
    class MemoryProvider final : public CE::Assets::ResourceProvider {
    public:
        [[nodiscard]] std::shared_ptr<CE::Assets::Image> load_image(const std::filesystem::path&) override {
            return std::make_shared<MemoryImage>(CE::Assets::PixelSize{32, 32});
        }
        [[nodiscard]] std::shared_ptr<CE::Assets::Image> create_font_atlas(std::span<const unsigned char>,
                                                                           CE::Assets::PixelSize size) override {
            return std::make_shared<MemoryImage>(size);
        }
        [[nodiscard]] std::shared_ptr<CE::Assets::Geometry2D>
        upload_geometry(std::shared_ptr<CE::Vertex2D> vertices, std::uint32_t count, CE::Assets::PrimitiveTopology topology) override {
            uploaded_vertices = vertices ? count : 0;
            uploaded_topology = topology;
            uploaded_geometry.clear();
            if (vertices)
                uploaded_geometry.assign(vertices.get(), vertices.get() + count);
            return geometry;
        }
        [[nodiscard]] std::shared_ptr<CE::Assets::Shader> link_program(const std::vector<std::filesystem::path>&) override {
            return shader;
        }

        std::uint32_t uploaded_vertices = 0;
        CE::Assets::PrimitiveTopology uploaded_topology = CE::Assets::PrimitiveTopology::Triangles;
        std::vector<CE::Vertex2D> uploaded_geometry;
        std::shared_ptr<MemoryGeometry> geometry = std::make_shared<MemoryGeometry>();
        std::shared_ptr<MemoryShader> shader = std::make_shared<MemoryShader>();
    };

    /** Records frame handoff while the test supplies its own display and input. */
    class MemoryRenderer final : public CE::RenderAPIs::iRenderer {
    public:
        void initialize() override { ++initializations; }
        void deinitialize() override { ++shutdowns; }
        void clear() override { ++clears; }
        void render(const CE::RenderAPIs::RenderFrame& frame) override {
            ++renders;
            last_pass_count = frame.passes().size();
            last_marked_pressed = !frame.passes().empty() && frame.passes().front().view[3][0] == 1.0f;
            render_thread = std::this_thread::get_id();
            if (on_render) on_render();
        }
        void set_viewport(CE::FramebufferSize size) override { viewport = size; }
        void set_depth_test(bool enabled) override { depth_enabled = enabled; }
        void set_clear_colour(float r, float g, float b, float a) override { clear_colour = {r, g, b, a}; }
        void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) override {
            camera_projection = projection;
            camera_view = view;
        }

        std::function<void()> on_render;
        CE::FramebufferSize viewport{};
        bool depth_enabled = false;
        glm::vec4 clear_colour{0.0f};
        glm::mat4 camera_projection{1.0f};
        glm::mat4 camera_view{1.0f};
        std::thread::id render_thread;
        std::size_t last_pass_count = 0;
        bool last_marked_pressed = false;
        int initializations = 0;
        int shutdowns = 0;
        int clears = 0;
        int renders = 0;
    };

    class MemorySurface final : public CE::RenderAPIs::iPresentationSurface {
    public:
        void present() override { ++presents; }
        int presents = 0;
    };

    // Construct the same owned adapter graph as the GLFW factory, with no native graphics API.
    std::unique_ptr<CE::Engine::EngineContext> make_test_context(MemoryInput& input,
                                                                  MemoryRenderer*& renderer,
                                                                  MemorySurface*& surface) {
        auto display = std::make_unique<MemoryDisplay>();
        auto* window = display->create_window(display->primary_monitor(), CE::Enum::window_mode::NORMAL, 320, 240);
        display->activate_window(*window);
        auto presentation = std::make_unique<MemorySurface>();
        surface = presentation.get();
        auto rendering = std::make_unique<MemoryRenderer>();
        renderer = rendering.get();
        return std::make_unique<CE::Engine::EngineContext>(std::move(display), std::move(presentation),
                                                            std::move(rendering), std::make_unique<MemoryProvider>(),
                                                            input);
    }
} // namespace

TEST(runtime_adapter, sequential_frame_from_completed_input) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game);
    game.on_tick = [&] { runtime.stop(); };
    input.on_poll = [&] { engine->window().resize(640, 360); };

    runtime.run();

    // The press is observed after the first poll; a frame is prepared and presented
    // using the completed simulation state before all adapters are detached.
    EXPECT_TRUE(game.pressed.load());
    EXPECT_GE(game.updates, 1);
    EXPECT_EQ(game.draws, 1);
    EXPECT_EQ(game.size, (CE::FramebufferSize{640, 360}));
    EXPECT_EQ(renderer->viewport, game.size);
    EXPECT_EQ(renderer->last_pass_count, 1u);
    EXPECT_EQ(renderer->renders, 1);
    EXPECT_EQ(surface->presents, 1);
    EXPECT_EQ(renderer->initializations, 1);
    EXPECT_EQ(renderer->shutdowns, 1);
    EXPECT_EQ(input.attached_window(), nullptr);
    EXPECT_EQ(game.update_thread, std::this_thread::get_id());
}

TEST(runtime_adapter, concurrent_simulation_presents_on_platform_thread) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game, CE::GFramework::RunMode::Concurrent);
    renderer->on_render = [&] { if (renderer->last_marked_pressed) runtime.stop(); };

    runtime.run();

    EXPECT_TRUE(game.pressed.load());
    EXPECT_NE(game.update_thread, std::this_thread::get_id());
    EXPECT_EQ(renderer->render_thread, std::this_thread::get_id());
    EXPECT_EQ(renderer->last_pass_count, 1u);
    EXPECT_GE(renderer->renders, 1);
    EXPECT_EQ(surface->presents, renderer->renders);
    EXPECT_TRUE(renderer->last_marked_pressed);
    EXPECT_EQ(renderer->shutdowns, 1);
    EXPECT_EQ(input.attached_window(), nullptr);
}

TEST(runtime_adapter, closing_before_an_update_still_shuts_down_adapters) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game);
    input.on_poll = [&] { static_cast<MemoryWindow&>(engine->window()).request_close(); };

    runtime.run();

    EXPECT_EQ(game.updates, 0);
    EXPECT_EQ(renderer->renders, 0);
    EXPECT_EQ(surface->presents, 0);
    EXPECT_EQ(renderer->shutdowns, 1);
    EXPECT_EQ(input.attached_window(), nullptr);
}

TEST(runtime_adapter, render_failure_still_shuts_down_adapters) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game);
    renderer->on_render = [] { throw std::runtime_error("recorded render failed"); };

    EXPECT_THROW(runtime.run(), std::runtime_error);
    EXPECT_EQ(renderer->shutdowns, 1);
    EXPECT_EQ(input.attached_window(), nullptr);
    EXPECT_EQ(surface->presents, 0);
}

TEST(runtime_adapter, sprite_cells_share_one_uploaded_grid) {
    MemoryProvider provider;
    const std::filesystem::path texture = "memory-adapter/sprite.png";
    CE::Assets::TextureMgr::get().load_assets({texture}, provider);
    CE::Assets::SpriteDefinition definition;
    definition.name_space = "memory-adapter";
    definition.name = "sprite";
    definition.texture = texture;
    definition.grid.frame = {16, 32};
    definition.grid.rows = 1;
    definition.grid.columns = 2;
    CE::Assets::SpriteMgr::get().load_assets({definition}, provider);
    const std::filesystem::path program = "memory-adapter/shader";
    CE::Assets::ShaderMgr::get().load_program(program, {"vertex", "fragment"}, provider);

    auto sprite = CE::Assets::SpriteMgr::get().get_asset(definition.id());
    auto shader = CE::Assets::ShaderMgr::get().get_asset(program);
    ASSERT_TRUE(sprite);
    ASSERT_TRUE(shader);

    CE::RenderAPIs::RenderFrame frame;
    CE::RenderAPIs::RenderFrameWriter writer(frame);
    auto pass = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
    CE::RenderAPIs::DrawStyle style;
    style.material = shader;
    pass.add(CE::RenderAPIs::SpriteDraw{sprite, 0, style});
    pass.add(CE::RenderAPIs::SpriteDraw{sprite, 1, style});

    EXPECT_EQ(provider.uploaded_vertices, 8u);
    EXPECT_EQ(provider.uploaded_topology, CE::Assets::PrimitiveTopology::TriangleStrip);
    ASSERT_EQ(frame.passes().size(), 1u);
    ASSERT_EQ(frame.passes()[0].draws.size(), 2u);
    const auto* first = std::get_if<CE::RenderAPIs::SpriteDraw>(&frame.passes()[0].draws[0]);
    const auto* second = std::get_if<CE::RenderAPIs::SpriteDraw>(&frame.passes()[0].draws[1]);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first->sprite, second->sprite);
    EXPECT_EQ(first->cell, 0u);
    EXPECT_EQ(second->cell, 1u);
    frame.recycle();
}

TEST(graphic, whole_image) {
    MemoryProvider provider;

    // Loading one image gives the graphic a pixel-sized six-vertex quad and
    // covers the complete texture, including its top-right UV corner.
    auto graphic = CE::Assets::Graphic::load(std::filesystem::path{"ui/panel.png"}, provider);
    ASSERT_EQ(provider.uploaded_vertices, 6u);
    EXPECT_EQ(provider.uploaded_topology, CE::Assets::PrimitiveTopology::Triangles);
    ASSERT_EQ(provider.uploaded_geometry.size(), std::size_t{6});
    EXPECT_FLOAT_EQ(provider.uploaded_geometry[0].x, 0.0f);
    EXPECT_FLOAT_EQ(provider.uploaded_geometry[0].y, -32.0f);
    EXPECT_FLOAT_EQ(provider.uploaded_geometry[2].u, 1.0f);
    EXPECT_FLOAT_EQ(provider.uploaded_geometry[2].v, 1.0f);
    EXPECT_FLOAT_EQ(provider.uploaded_geometry[5].x, 0.0f);

    // A single submission draws all six vertices with the loaded image.
    CE::DrawInfo info;
    info.material = provider.shader;
    graphic.draw(info);
    EXPECT_EQ(provider.geometry->bound_size.width, 32u);
    EXPECT_EQ(provider.geometry->bound_size.height, 32u);
    EXPECT_EQ(provider.geometry->first_vertex, 0u);
    EXPECT_EQ(provider.geometry->drawn_vertices, 6u);
    EXPECT_EQ(provider.shader->uses, 1);
}
