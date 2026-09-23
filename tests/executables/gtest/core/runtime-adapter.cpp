#include <gtest/gtest.h>

#include <assets/abstracts/resource-provider.h>
#include <assets/primitives/draw-info.h>
#include <core/engines/runtime-engine.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>

#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error The alternative runtime adapter must not include OpenGL or GLFW headers.
#endif

namespace {
    constexpr CE::Input::DeviceButtonId test_button = 65;

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

    class MemoryInput final : public CE::Input::iInputSystem {
    public:
        void initialize(CE::iWindow& window) override { window_ = &window; }
        void poll() override { bindings_.on_button({keyboard_id(), test_button}, false, true); }
        void deinitialize() override { window_ = nullptr; }
        [[nodiscard]] CE::Input::InputBindings& bindings() override { return bindings_; }
        [[nodiscard]] CE::Input::DeviceId keyboard_id() const override { return 1; }
        [[nodiscard]] CE::Input::DeviceId mouse_id() const override { return 2; }
        [[nodiscard]] CE::Input::DeviceId gamepad_id() const override { return 3; }
        [[nodiscard]] CE::iWindow* attached_window() const { return window_; }

    private:
        CE::iWindow* window_ = nullptr;
        CE::Input::InputBindings bindings_;
    };

    class MemoryImage final : public CE::Assets::Image {
    public:
        explicit MemoryImage(CE::Assets::PixelSize size) : size_(size) {}
        [[nodiscard]] CE::Assets::PixelSize pixel_size() const override { return size_; }

    private:
        CE::Assets::PixelSize size_;
    };

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

    class MemoryProvider final : public CE::Assets::ResourceProvider {
    public:
        [[nodiscard]] std::shared_ptr<CE::Assets::Image> load_image(const std::filesystem::path&) override {
            return std::make_shared<MemoryImage>(CE::Assets::PixelSize{32, 32});
        }
        [[nodiscard]] std::shared_ptr<CE::Assets::Image> create_font_atlas(
            std::span<const unsigned char>, CE::Assets::PixelSize size) override {
            return std::make_shared<MemoryImage>(size);
        }
        [[nodiscard]] std::shared_ptr<CE::Assets::Geometry2D> upload_geometry(
            std::shared_ptr<CE::Vertex2D> vertices, std::uint32_t count) override {
            uploaded_vertices = vertices ? count : 0;
            return geometry;
        }
        [[nodiscard]] std::shared_ptr<CE::Assets::Shader> compile_stage(const std::filesystem::path&) override {
            return shader;
        }
        [[nodiscard]] std::shared_ptr<CE::Assets::Shader> link_program(
            const std::vector<std::filesystem::path>&) override {
            return shader;
        }

        std::uint32_t uploaded_vertices = 0;
        std::shared_ptr<MemoryGeometry> geometry = std::make_shared<MemoryGeometry>();
        std::shared_ptr<MemoryShader> shader = std::make_shared<MemoryShader>();
    };

    class MemoryRenderer final : public CE::RenderAPIs::iRenderer {
    public:
        explicit MemoryRenderer(MemoryProvider& provider) : provider_(provider) {}
        void initialize_libraries() override {
            auto memory = std::make_unique<MemoryDisplay>();
            auto* window = memory->create_window(memory->primary_monitor(), CE::Enum::window_mode::NORMAL, 320, 240);
            memory->activate_window(*window);
            display = std::move(memory);
        }
        void initialize_rendering_context() override {}
        void deinitialize() override {}
        void clear() override { ++clears; }
        void set_viewport(CE::FramebufferSize size) override { viewport = size; }
        void set_depth_test(bool enabled) override { depth_enabled = enabled; }
        void set_clear_colour(float r, float g, float b, float a) override { clear_colour = {r, g, b, a}; }
        void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) override {
            CE::Assets::ShaderMgr::get().set_camera_matrices(projection, view);
        }
        void swap_buffer() override { ++swaps; }
        [[nodiscard]] CE::Assets::ResourceProvider& resources() override { return provider_; }
        [[nodiscard]] MemoryWindow& window() { return static_cast<MemoryDisplay&>(*display).window(); }

        CE::FramebufferSize viewport{};
        bool depth_enabled = false;
        glm::vec4 clear_colour{0.0f};
        int clears = 0;
        int swaps = 0;

    private:
        MemoryProvider& provider_;
    };
}

TEST(runtime_adapter, runs_input_resize_loading_and_draw_through_an_alternative_backend) {
    // Asset managers keep singleton caches, so their provider must outlive the test.
    static MemoryProvider provider;
    MemoryRenderer renderer(provider);
    MemoryInput input;
    CE::Engine::RuntimeEngine engine(renderer, [&]() -> CE::Input::iInputSystem& { return input; });
    engine.init();
    ASSERT_EQ(input.attached_window(), renderer.display->active_window());

    bool pressed = false;
    input.bindings().bind_button({input.keyboard_id(), test_button},
                                 [&](bool, bool current) { pressed = current; });
    engine.poll_input();
    EXPECT_TRUE(pressed);

    engine.set_clear_colour(0.1f, 0.2f, 0.3f, 1.0f);
    EXPECT_FLOAT_EQ(renderer.clear_colour.r, 0.1f);
    engine.hide_cursor(true);
    EXPECT_TRUE(renderer.window().cursor_hidden());
    engine.set_mode(CE::Enum::window_mode::BORDERLESS);
    EXPECT_EQ(renderer.window().mode(), CE::Enum::window_mode::BORDERLESS);
    renderer.window().resize(640, 360);
    engine.pre_draw();
    EXPECT_EQ(renderer.viewport, (CE::FramebufferSize{640, 360}));
    EXPECT_EQ(engine.active_camera()->framebuffer_size(), renderer.viewport);
    EXPECT_EQ(renderer.clears, 1);
    engine.set_mode(CE::Enum::gfx_mode::R3D);
    EXPECT_TRUE(renderer.depth_enabled);
    engine.set_mode(CE::Enum::gfx_mode::R2D);
    EXPECT_FALSE(renderer.depth_enabled);

    const std::filesystem::path texture = "memory-adapter/sprite.png";
    auto& resources = engine.resources();
    CE::Assets::TextureMgr::get().load_assets({texture}, resources);
    CE::Assets::SpriteDefinition definition;
    definition.name_space = "memory-adapter";
    definition.name = "sprite";
    definition.texture = texture;
    definition.grid.frame = {32, 32};
    definition.grid.rows = 1;
    definition.grid.columns = 1;
    CE::Assets::SpriteMgr::get().load_assets({definition}, resources);
    const std::filesystem::path program = "memory-adapter/shader";
    CE::Assets::ShaderMgr::get().load_program(program, {"vertex", "fragment"}, resources);

    auto sprite = CE::Assets::SpriteMgr::get().get_asset(definition.id());
    auto shader = CE::Assets::ShaderMgr::get().get_asset(program);
    ASSERT_TRUE(sprite);
    ASSERT_TRUE(shader);
    CE::DrawInfo draw;
    draw.material = shader;
    sprite->draw(draw);
    engine.post_draw();
    EXPECT_EQ(provider.uploaded_vertices, 6u);
    EXPECT_EQ(provider.geometry->drawn_vertices, 6u);
    EXPECT_EQ(provider.geometry->bound_size.width, 32u);
    EXPECT_EQ(provider.shader->uses, 2);
    EXPECT_EQ(provider.shader->projection, engine.active_camera()->projection_matrix());
    EXPECT_EQ(renderer.swaps, 1);

    renderer.window().request_close();
    EXPECT_TRUE(engine.should_close());
    engine.deinit();
    EXPECT_EQ(input.attached_window(), nullptr);
}
