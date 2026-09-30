#include <gtest/gtest.h>

#include <assets/resources/resource-provider.h>
#include <assets/types/2d/graphic.h>
#include <core/controls/input-interface.h>
#include <core/display/display-system-interface.h>
#include <core/display/window-interface.h>
#include <core/engine/engine-context.h>
#include <core/game-framework/abstract-game.h>
#include <core/game-framework/game-runtime.h>
#include <core/rendering/draw-info.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <core/resources/asset-management/asset-loader.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <internals/exceptions.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
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
    struct TemporaryAssets {
        std::filesystem::path root;
        inline static std::atomic<unsigned int> next{0};

        TemporaryAssets()
        : root(std::filesystem::temp_directory_path() /
            ("cheryl-preparation-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
                + "-" + std::to_string(next.fetch_add(1)))
        ) {
            std::filesystem::create_directories(root);
        }

        ~TemporaryAssets() {
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }

        void write_png(const std::string& name = "pixel.png") const {
            constexpr std::array<unsigned char, 70> bytes{
                137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 1, 0, 0, 0, 1, 8, 6, 0, 0, 0,
                31, 21, 196, 137, 0, 0, 0, 13, 73, 68, 65, 84, 120, 156, 99, 248, 207, 192, 240, 31, 0, 5, 0, 1, 255,
                137, 153, 61, 29, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130
            };
            const auto file = root / name;
            std::filesystem::create_directories(file.parent_path());
            std::ofstream output(file, std::ios::binary);
            output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }

        void write_manifest(const std::string& name_space = "probe") const {
            std::string document =
                R"JSON({"$schema":"./schemas/asset-manifest-1.0.schema.json","version":"1.0","namespace":"probe","defaults":{"sprite":{"pivot":{"x":0.5,"y":1.0}},"tileset":{"pivot":{"x":0.5,"y":0.5}}},"texture":"pixel.png","sprites":{"pixel":{"grid":{"origin":{"x":0,"y":0},"frame":{"width":1,"height":1},"spacing":{"x":0,"y":0},"rows":1,"columns":1,"cell_order":"row-major"}}}})JSON";
            document.replace(document.find("probe"), 5, name_space);
            std::ofstream(root / "probe.json") << document;
        }
    };

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
        std::function<void()> on_destroy;

        ~MemoryDisplay() override {
            if (on_destroy)
                on_destroy();
        }

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
        std::function<void()> on_destroy;

        ~MemoryInput() override {
            if (on_destroy)
                on_destroy();
        }

        bool default_press = true;
        void initialize(CE::iWindow& window) override { window_ = &window; }

        void poll() override {
            begin_input_poll();
            if (on_poll)
                on_poll();
            if (default_press)
                bindings_.on_button({keyboard_id(), test_button}, true);
            (void)publish_input();
        }

        void deinitialize() override {
            window_ = nullptr;
            discard_captured_input();
            bindings_.clear();
        }

        void key(CE::Input::ButtonPhase phase) {
            capture_buffer().record(keyboard_id(), CE::Input::DeviceKind::Keyboard, CE::Input::ButtonEvent{test_button, phase});
            if (phase != CE::Input::ButtonPhase::Repeat)
                bindings_.on_button({keyboard_id(), test_button}, phase == CE::Input::ButtonPhase::Press);
        }

        void text(char32_t codepoint) {
            capture_buffer().record(keyboard_id(), CE::Input::DeviceKind::Keyboard, CE::Input::TextEvent{codepoint});
        }

        [[nodiscard]] bool supports(CE::Input::InputMode) const override { return true; }
        [[nodiscard]] bool supports_focus() const override { return true; }
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
        bool capture_;
        CE::Input::CaptureLease events_;
        CE::Input::CaptureLease text_;
        CE::Input::FocusLease focus_;

    public:
        std::function<void()> on_tick;
        std::function<void()> on_init;
        std::function<void()> on_deinit;
        int initializations = 0;
        int shutdowns = 0;
        int updates = 0;
        mutable int draws = 0;
        std::atomic<bool> pressed{false};
        std::thread::id update_thread;
        CE::FramebufferSize size{};
        std::vector<CE::Input::InputRecord> received_records;

        explicit OneTickGame(CE::Input::iInputSystem& input, const bool capture = false)
        : input_(input), capture_(capture) {}

        void init() override {
            ++initializations;
            (void)input_.bindings().bind_button({input_.keyboard_id(), test_button}, action);
            if (capture_) {
                events_ = input_.capture(CE::Input::InputMode::Events);
                text_ = input_.capture(CE::Input::InputMode::Text);
                focus_ = input_.routing().focus(29, CE::Input::KeyboardRouting::PassThrough);
            }
            if (on_init)
                on_init();
        }

        void deinit() override {
            ++shutdowns;
            focus_.reset();
            text_.reset();
            events_.reset();
            input_.bindings().clear();
            if (on_deinit)
                on_deinit();
        }

        void update(const CE::GFramework::TickContext& tick) override {
            ++updates;
            update_thread = std::this_thread::get_id();
            size = tick.framebuffer_size;
            received_records.insert(received_records.end(), tick.input.records().begin(), tick.input.records().end());
            if (tick.input.button(action).pressed())
                pressed.store(true);
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
        explicit MemoryImage(CE::Assets::PixelSize size)
        : size_(size) {}

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
        void bind_pass(const CE::Assets::ShaderPass& pass) override {
            use();
            projection = pass.projection;
            view = pass.view;
        }

        void bind_draw(const CE::Assets::ShaderDraw& draw) override { last_draw = draw; }
        void use() override { ++uses; }
        void set_uniform_value(const char*, float) override { ++raw_uniform_writes; }
        void set_uniform_value(const char*, int) override { ++raw_uniform_writes; }
        void set_uniform_value(const char*, unsigned int) override { ++raw_uniform_writes; }
        void set_uniform_value(const char*, bool) override { ++raw_uniform_writes; }
        void set_uniform_matrix(const char*, const glm::mat4&) override { ++raw_uniform_writes; }
        CE::Assets::ShaderDraw last_draw;
        int raw_uniform_writes = 0;
        int uses = 0;
        glm::mat4 projection{0.0f};
        glm::mat4 view{0.0f};
    };

    /** Supplies in-memory assets and records the geometry uploaded by asset managers. */
    class MemoryProvider final : public CE::Assets::ResourceProvider {
    public:
        [[nodiscard]] std::shared_ptr<CE::Assets::Image> load_image(const std::filesystem::path&) override {
            if (on_load_image)
                return on_load_image();
            return std::make_shared<MemoryImage>(CE::Assets::PixelSize{32, 32});
        }

        [[nodiscard]] std::shared_ptr<CE::Assets::Image> create_font_atlas(
            std::span<const unsigned char>,
            CE::Assets::PixelSize size
        ) override {
            resource_thread = std::this_thread::get_id();
            ++atlas_uploads;
            return std::make_shared<MemoryImage>(size);
        }

        [[nodiscard]] std::shared_ptr<CE::Assets::Image> create_image(const CE::Assets::DecodedImage& image) override {
            ++created_images;
            return std::make_shared<MemoryImage>(image.size);
        }

        using ResourceProvider::upload_geometry;

        [[nodiscard]] std::shared_ptr<CE::Assets::Geometry2D> upload_geometry(
            std::span<const CE::Vertex2D> vertices,
            CE::Assets::PrimitiveTopology topology
        ) override {
            uploaded_vertices = vertices.size();
            uploaded_topology = topology;
            uploaded_geometry.assign(vertices.begin(), vertices.end());
            return geometry;
        }

        [[nodiscard]] std::shared_ptr<CE::Assets::Shader> link_program(const std::vector<std::filesystem::path>&) override {
            ++linked_programs;
            return shader;
        }

        std::uint32_t uploaded_vertices = 0;
        std::thread::id resource_thread;
        int atlas_uploads = 0;
        int linked_programs = 0;
        int created_images = 0;
        std::function<std::shared_ptr<CE::Assets::Image>()> on_load_image;
        CE::Assets::PrimitiveTopology uploaded_topology = CE::Assets::PrimitiveTopology::Triangles;
        std::vector<CE::Vertex2D> uploaded_geometry;
        std::shared_ptr<MemoryGeometry> geometry = std::make_shared<MemoryGeometry>();
        std::shared_ptr<MemoryShader> shader = std::make_shared<MemoryShader>();
    };

    /** Records frame handoff while the test supplies its own display and input. */
    class MemoryRenderer final : public CE::RenderAPIs::iRenderer {
    public:
        void initialize() override {
            ++initializations;
            if (on_initialize)
                on_initialize();
        }

        void deinitialize() override { ++shutdowns; }
        void clear() override { ++clears; }

        void render(const CE::RenderAPIs::RenderFrame& frame) override {
            ++renders;
            last_pass_count = frame.passes().size();
            last_marked_pressed = !frame.passes().empty() && frame.passes().front().view[3][0] == 1.0f;
            render_thread = std::this_thread::get_id();
            if (on_render)
                on_render();
        }

        void set_viewport(CE::FramebufferSize size) override { viewport = size; }
        void set_depth_test(bool enabled) override { depth_enabled = enabled; }
        void set_clear_colour(float r, float g, float b, float a) override { clear_colour = {r, g, b, a}; }

        void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) override {
            camera_projection = projection;
            camera_view = view;
        }

        std::function<void()> on_render;
        std::function<void()> on_initialize;
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
    std::unique_ptr<CE::Engine::EngineContext> make_test_context(MemoryInput& input, MemoryRenderer*& renderer, MemorySurface*& surface) {
        auto display = std::make_unique<MemoryDisplay>();
        auto* window = display->create_window(display->primary_monitor(), CE::Enum::window_mode::NORMAL, 320, 240);
        display->activate_window(*window);
        auto presentation = std::make_unique<MemorySurface>();
        surface = presentation.get();
        auto rendering = std::make_unique<MemoryRenderer>();
        renderer = rendering.get();
        return std::make_unique<CE::Engine::EngineContext>(std::move(display), std::move(presentation), std::move(rendering),
            std::make_unique<MemoryProvider>(), input);
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
    EXPECT_EQ(game.updates, 1);
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

TEST(runtime_adapter, partial_game_initialization_is_cleaned_up_in_both_modes) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input, true);
        game.on_init = [] { throw std::runtime_error("game initialization failed"); };
        game.on_deinit = [] { throw std::runtime_error("cleanup also failed"); };
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        try {
            runtime.run();
            FAIL() << "Initialization must fail";
        } catch (const std::runtime_error& failure) {
            EXPECT_STREQ(failure.what(), "game initialization failed");
        }
        EXPECT_EQ(game.initializations, 1);
        EXPECT_EQ(game.shutdowns, 1);
        EXPECT_EQ(input.routing().current()->target, 0u);
        EXPECT_EQ(input.attached_window(), nullptr);
        EXPECT_EQ(renderer->shutdowns, 1);
        EXPECT_THROW(runtime.run(), CE::Exceptions::failed_operation);
    }
}

TEST(runtime_adapter, failed_renderer_initialization_does_not_start_the_game) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    renderer->on_initialize = [] { throw std::runtime_error("renderer initialization failed"); };
    CE::GFramework::GameRuntime runtime(*engine, game);
    EXPECT_THROW(runtime.run(), std::runtime_error);
    EXPECT_EQ(renderer->shutdowns, 1);
    EXPECT_EQ(game.initializations, 0);
    EXPECT_EQ(game.shutdowns, 0);
}

TEST(runtime_adapter, a_stopped_adapter_graph_cannot_be_started_by_another_runtime) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::GameRuntime first(*engine, game);
    game.on_tick = [&] { first.stop(); };
    first.run();
    CE::GFramework::GameRuntime second(*engine, game);
    EXPECT_THROW(second.run(), CE::Exceptions::failed_operation);
    EXPECT_THROW(first.run(), CE::Exceptions::failed_operation);
    EXPECT_EQ(renderer->initializations, 1);
    EXPECT_EQ(renderer->shutdowns, 1);
}

TEST(runtime_adapter, owned_input_is_destroyed_while_its_window_is_alive) {
    bool display_alive = true;
    bool input_destroyed = false;
    auto display = std::make_unique<MemoryDisplay>();
    display->on_destroy = [&] { display_alive = false; };
    auto* window = display->create_window(display->primary_monitor(), CE::Enum::window_mode::NORMAL, 320, 240);
    display->activate_window(*window);
    auto input = std::make_unique<MemoryInput>();
    input->on_destroy = [&] {
        input_destroyed = true;
        EXPECT_TRUE(display_alive);
    };
    auto engine = std::make_unique<CE::Engine::EngineContext>(std::move(display), std::make_unique<MemorySurface>(),
        std::make_unique<MemoryRenderer>(), std::make_unique<MemoryProvider>(),
        std::move(input));
    engine.reset();
    EXPECT_TRUE(input_destroyed);
    EXPECT_FALSE(display_alive);
}

TEST(platform_requests, simulation_transfers_owned_pixels_to_the_platform) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        std::future<std::shared_ptr<CE::Assets::Image>> pending;
        std::shared_ptr<CE::Assets::Image> image;
        game.on_tick = [&] {
            if (!pending.valid()) {
                auto pixels = std::make_unique<std::vector<unsigned char>>(4, 255);
                pending = engine->platform_dispatcher().submit([pixels = std::move(pixels)](CE::Engine::EngineContext& platform) {
                    return platform.resources().create_font_atlas(*pixels, {2, 2});
                });
            } else if (pending.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
                image = pending.get();
                runtime.stop();
            }
        };
        runtime.run();
        ASSERT_TRUE(image);
        EXPECT_EQ(image->pixel_size().width, 2u);
        const auto& provider = static_cast<MemoryProvider&>(engine->resources());
        EXPECT_EQ(provider.atlas_uploads, 1);
        EXPECT_EQ(provider.resource_thread, std::this_thread::get_id());
        if (mode == CE::GFramework::RunMode::Concurrent)
            EXPECT_NE(game.update_thread, provider.resource_thread);
        EXPECT_THROW(static_cast<void>(engine->platform_dispatcher().submit([](CE::Engine::EngineContext&) { return 1; })),
            CE::Exceptions::failed_operation);
    }
}

TEST(platform_requests, one_failed_callback_does_not_abort_another_request) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game);
    std::future<int> failure;
    std::future<int> success;
    game.on_init = [&] {
        failure = engine->platform_dispatcher().submit([](
            CE::Engine::EngineContext&



        ) ->
            int {
                throw std::runtime_error("request failed");
            });
        success = engine->platform_dispatcher().submit([](CE::Engine::EngineContext&) { return 17; });
    };
    game.on_tick = [&] {
        EXPECT_THROW(failure.get(), std::runtime_error);
        EXPECT_EQ(success.get(), 17);
        runtime.stop();
    };
    runtime.run();
    EXPECT_EQ(game.shutdowns, 1);
}

TEST(platform_requests, shutdown_cancels_pending_captures_before_game_cleanup) {
    struct CapturedData {
        bool& destroyed;
        std::thread::id& thread;

        ~CapturedData() {
            destroyed = true;
            thread = std::this_thread::get_id();
        }
    };
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    EXPECT_THROW(static_cast<void>(engine->platform_dispatcher().submit([](CE::Engine::EngineContext&) {})), CE::Exceptions::failed_operation);
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game);
    std::future<int> pending;
    bool destroyed = false;
    std::thread::id destruction_thread;
    game.on_init = [&] {
        auto data = std::make_unique<CapturedData>(destroyed, destruction_thread);
        pending = engine->platform_dispatcher().submit([data = std::move(data)](CE::Engine::EngineContext&) { return 1; });
        runtime.stop();
    };
    game.on_deinit = [&] { EXPECT_TRUE(destroyed); };
    runtime.run();
    EXPECT_EQ(destruction_thread, std::this_thread::get_id());
    try {
        (void)pending.get();
        FAIL() << "The unexecuted request must be cancelled";
    } catch (const std::future_error& error) {
        EXPECT_EQ(error.code(), std::make_error_code(std::future_errc::broken_promise));
    }
}

TEST(runtime_adapter, concurrent_simulation_presents_on_platform_thread) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game, CE::GFramework::RunMode::Concurrent);
    renderer->on_render = [&] {
        if (renderer->last_marked_pressed)
            runtime.stop();
    };

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

TEST(runtime_adapter, sequential_and_concurrent_handoffs_preserve_routed_event_and_text_order) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        input.default_press = false;
        bool emitted = false;
        input.on_poll = [&] {
            if (std::exchange(emitted, true))
                return;
            input.key(CE::Input::ButtonPhase::Press);
            input.text(U'\u00e9');
            input.key(CE::Input::ButtonPhase::Repeat);
            input.text(U'\u00e9');
            input.key(CE::Input::ButtonPhase::Release);
        };
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input, true);
        const CE::Input::PollingOptions polling{CE::Input::PollingPolicy::Finite, 3, std::chrono::milliseconds(1)};
        CE::GFramework::GameRuntime runtime(*engine, game, mode, polling);
        renderer->on_render = [&] {
            if (renderer->last_marked_pressed)
                runtime.stop();
        };
        runtime.run();

        // These results are inspected only after the runtime has joined its worker.
        ASSERT_EQ(game.received_records.size(), 5u);
        EXPECT_EQ(std::get<CE::Input::ButtonEvent>(game.received_records[0].data).phase, CE::Input::ButtonPhase::Press);
        EXPECT_EQ(std::get<CE::Input::TextEvent>(game.received_records[1].data).codepoint, U'\u00e9');
        EXPECT_EQ(std::get<CE::Input::ButtonEvent>(game.received_records[2].data).phase, CE::Input::ButtonPhase::Repeat);
        EXPECT_EQ(std::get<CE::Input::TextEvent>(game.received_records[3].data).codepoint, U'\u00e9');
        EXPECT_EQ(std::get<CE::Input::ButtonEvent>(game.received_records[4].data).phase, CE::Input::ButtonPhase::Release);
        for (const auto& record : game.received_records) {
            EXPECT_EQ(record.target, 29u);
            EXPECT_NE(record.focus_epoch, 0u);
            EXPECT_TRUE(record.to_gameplay);
        }
        EXPECT_TRUE(game.pressed.load());
        EXPECT_EQ(input.attached_window(), nullptr);
        EXPECT_EQ(renderer->shutdowns, 1);
    }
}

TEST(runtime_adapter, poll_failure_shuts_down_both_runtime_modes_and_active_capture) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input, true);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        input.on_poll = [] { throw std::runtime_error("recorded poll failed"); };
        EXPECT_THROW(runtime.run(), std::runtime_error);
        EXPECT_EQ(input.attached_window(), nullptr);
        EXPECT_EQ(input.routing().current()->target, 0u);
        EXPECT_EQ(renderer->shutdowns, 1);
    }
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

TEST(material_cache, linking_does_not_bind_draw_state_and_reload_preserves_old_handles) {
    MemoryProvider provider;
    auto& shaders = CE::Assets::ShaderMgr::get();
    const std::filesystem::path key{"material-probe"};
    shaders.load_program(key, {"vertex", "fragment"}, provider);
    auto original = shaders.get_asset(key);
    EXPECT_EQ(provider.linked_programs, 1);
    EXPECT_EQ(provider.shader->uses, 0);
    EXPECT_EQ(provider.shader->raw_uniform_writes, 0);
    shaders.load_program(key, {"vertex", "fragment"}, provider);
    EXPECT_EQ(provider.linked_programs, 1);
    provider.shader = std::make_shared<MemoryShader>();
    shaders.reload_program(key, {"vertex", "fragment"}, provider);
    EXPECT_EQ(provider.linked_programs, 2);
    EXPECT_NE(original, shaders.get_asset(key));
    EXPECT_TRUE(original);
    EXPECT_EQ(provider.shader->uses, 0);
    auto replacement = shaders.get_asset(key);
    provider.shader.reset();
    EXPECT_THROW(shaders.reload_program(key, {"vertex", "fragment"}, provider), CE::Exceptions::failed_operation);
    EXPECT_EQ(shaders.get_asset(key), replacement);
}

TEST(asset_cache, readers_keep_complete_handles_while_assets_are_published) {
    struct Cache : CE::Assets::AssetMgr<MemoryImage, int> {
        void publish(int key) { publish_asset(key, std::make_shared<MemoryImage>(CE::Assets::PixelSize{1, 1})); }
    } cache;
    std::atomic<bool> finished{false};
    auto reader = std::async(std::launch::async, [&] {
        bool complete = true;
        do {
            for (int key = 0; key < 100; ++key) {
                const auto image = cache.get_asset(key);
                if (image)
                    complete = complete && image->pixel_size().width == 1;
                (void)cache.contains(key);
                (void)cache.size();
            }
        } while (!finished.load(std::memory_order_acquire));
        return complete;
    });
    for (int key = 0; key < 100; ++key)
        cache.publish(key);
    finished.store(true, std::memory_order_release);
    EXPECT_TRUE(reader.get());
    auto retained = cache.get_asset(7);
    cache.clear_assets();
    EXPECT_EQ(cache.size(), 0u);
    ASSERT_TRUE(retained);
    EXPECT_EQ(retained->pixel_size().width, 1u);
}

TEST(asset_cache, final_asset_release_can_inspect_the_cleared_cache) {
    struct Cache : CE::Assets::AssetMgr<MemoryImage, int> {
        void publish(std::shared_ptr<MemoryImage> image) { publish_asset(1, std::move(image)); }
    } cache;
    std::size_t size_at_deletion = 99;
    cache.publish(std::shared_ptr<MemoryImage>(new MemoryImage({1, 1}), [&](MemoryImage* image) {
        size_at_deletion = cache.size();
        delete image;
    }));
    cache.clear_assets();
    EXPECT_EQ(size_at_deletion, 0u);
}

TEST(asset_cache, provider_loads_reject_another_thread_and_teardown_refills) {
    bool refill_rejected = false;
    auto provider = std::make_unique<MemoryProvider>();
    auto* owner = provider.get();
    provider->on_load_image = [&] {
        return std::shared_ptr<MemoryImage>(new MemoryImage({1, 1}), [&](MemoryImage* image) {
            try {
                CE::Assets::TextureMgr::get().load_assets({"late-refill.png"}, *owner);
            } catch (const CE::Exceptions::failed_operation&) {
                refill_rejected = true;
            }
            delete image;
        });
    };
    auto& textures = CE::Assets::TextureMgr::get();
    textures.load_assets({"owner-thread.png"}, *provider);
    auto another_thread = std::async(std::launch::async, [&] {
        try {
            textures.load_assets({"wrong-thread.png"}, *provider);
        } catch (const CE::Exceptions::failed_operation&) {
            return true;
        }
        return false;
    });
    EXPECT_TRUE(another_thread.get());
    EXPECT_EQ(textures.size(), 1u);
    provider.reset();
    EXPECT_TRUE(refill_rejected);
    EXPECT_EQ(textures.size(), 0u);
}

TEST(asset_preparation, worker_decoding_owns_pixels_that_upload_without_reopening_files) {
    TemporaryAssets files;
    files.write_png();
    files.write_manifest();
    CE::Assets::Loader loader(files.root);
    auto worker = std::async(std::launch::async, [&] { return loader.prepare(); });
    auto prepared = worker.get();
    ASSERT_EQ(prepared.images.size(), 1u);
    EXPECT_EQ(prepared.images[0].pixels.rgba, (std::vector<unsigned char>{255, 0, 0, 255}));
    EXPECT_TRUE(loader.manifests()->empty());
    std::filesystem::remove(files.root / "pixel.png");
    MemoryProvider provider;
    loader.upload(std::move(prepared), provider);
    EXPECT_EQ(provider.created_images, 1);
    EXPECT_EQ(provider.linked_programs, 0);
    auto sprite = CE::Assets::SpriteMgr::get().get_asset("probe:pixel");
    ASSERT_TRUE(sprite);
    EXPECT_EQ(sprite->texture->pixel_size().width, 1u);
    EXPECT_EQ(loader.manifests()->size(), 1u);
}

TEST(asset_preparation, roots_and_metadata_snapshots_remain_independent_across_fresh_scans) {
    TemporaryAssets first;
    TemporaryAssets second;
    first.write_png();
    first.write_manifest("first");
    second.write_png();
    second.write_manifest("second");
    CE::Assets::Loader loader(first.root);
    CE::Assets::Loader other(second.root);
    EXPECT_EQ(loader.prepare().images.size(), 1u);
    first.write_png("new.png");
    EXPECT_EQ(loader.prepare().images.size(), 2u);
    EXPECT_EQ(other.prepare().images.size(), 1u);
    MemoryProvider provider;
    loader.load_assets(provider);
    const auto retained = loader.manifests();
    ASSERT_EQ(retained->size(), 1u);
    first.write_manifest("revised");
    loader.load_assets(provider);
    const auto current = loader.manifests();
    EXPECT_NE(current, retained);
    EXPECT_EQ(retained->front().name_space, "first");
    EXPECT_EQ(current->front().name_space, "revised");
    std::ofstream(first.root / "pixel.png", std::ios::binary) << "corrupt image";
    EXPECT_THROW(loader.load_assets(provider), CE::Exceptions::runtime_exception);
    EXPECT_EQ(loader.manifests(), current);
}

TEST(resource_upload, a_legacy_vertex_owner_is_released_after_the_transient_copy) {
    MemoryProvider provider;
    auto quad = std::make_shared<CE::Quad>();
    quad->vertices[0].x = 7.0f;
    std::weak_ptr<CE::Quad> owner = quad;
    (void)provider.upload_geometry(std::shared_ptr<CE::Vertex2D>{quad, quad->vertices.data()}, quad->vertices.size(),
        CE::Assets::PrimitiveTopology::Triangles);
    quad.reset();
    EXPECT_TRUE(owner.expired());
    ASSERT_EQ(provider.uploaded_geometry.size(), 6u);
    EXPECT_FLOAT_EQ(provider.uploaded_geometry.front().x, 7.0f);
}

TEST(platform_requests, a_saved_submission_endpoint_rejects_after_context_destruction) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    auto endpoint = engine->platform_dispatcher().submission();
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game);
    game.on_tick = [&] { runtime.stop(); };
    runtime.run();
    engine.reset();
    EXPECT_THROW(static_cast<void>(endpoint.submit([](CE::Engine::EngineContext&) {})), CE::Exceptions::failed_operation);
}

TEST(platform_requests, posting_during_a_drain_defers_work_to_the_next_drain) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game);
    std::future<void> outer;
    std::future<void> inner;
    std::vector<int> order;
    game.on_init = [&] {
        auto endpoint = engine->platform_dispatcher().submission();
        outer = endpoint.submit([&, endpoint](CE::Engine::EngineContext&) {
            order.push_back(1);
            inner = endpoint.submit([&](CE::Engine::EngineContext&) { order.push_back(3); });
            // Queueing on the owner must still defer the new callback.
            EXPECT_EQ(order, std::vector<int>{1});
            EXPECT_EQ(inner.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
            order.push_back(2);
        });
    };
    game.on_tick = [&] { runtime.stop(); };
    runtime.run();
    outer.get();
    inner.get();
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(simulation_requests, mailbox_work_precedes_update_on_the_simulation_owner) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        auto endpoint = runtime.simulation_dispatcher().submission();
        std::future<int> completed;
        std::thread::id delivery_thread;
        int value = 0;
        EXPECT_THROW(static_cast<void>(endpoint.submit([] {})), CE::Exceptions::failed_operation);
        game.on_init = [&] {
            auto owned = std::make_unique<int>(42);
            completed = endpoint.submit([&, owned = std::move(owned)] {
                delivery_thread = std::this_thread::get_id();
                value = *owned;
                return value;
            });
        };
        game.on_tick = [&] {
            EXPECT_EQ(value, 42);
            EXPECT_EQ(delivery_thread, std::this_thread::get_id());
            EXPECT_EQ(completed.get(), 42);
            runtime.stop();
        };
        runtime.run();
        EXPECT_EQ(delivery_thread, game.update_thread);
        if (mode == CE::GFramework::RunMode::Concurrent)
            EXPECT_NE(delivery_thread, std::this_thread::get_id());
        EXPECT_THROW(static_cast<void>(endpoint.submit([] {})), CE::Exceptions::failed_operation);
    }
}

TEST(simulation_requests, a_reentrant_post_waits_for_the_next_update_boundary) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        auto endpoint = runtime.simulation_dispatcher().submission();
        std::future<void> outer;
        std::future<void> inner;
        std::vector<int> order;
        int updates = 0;
        game.on_init = [&] {
            outer = endpoint.submit([&] {
                order.push_back(1);
                inner = endpoint.submit([&] { order.push_back(3); });
            });
        };
        game.on_tick = [&] {
            if (++updates == 1) {
                EXPECT_EQ(order, std::vector<int>{1});
                EXPECT_EQ(inner.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
                order.push_back(2);
            }
            else {
                EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
                runtime.stop();
            }
        };
        runtime.run();
        outer.get();
        inner.get();
        EXPECT_EQ(updates, 2);
    }
}

TEST(simulation_requests, shutdown_cancels_pending_captures_on_the_simulation_owner) {
    struct CapturedData {
        bool& destroyed;
        std::thread::id& destruction_thread;
        ~CapturedData() {
            destroyed = true;
            destruction_thread = std::this_thread::get_id();
        }
    };
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        std::future<void> cancelled;
        bool destroyed = false;
        std::thread::id destruction_thread;
        game.on_tick = [&] {
            auto data = std::make_unique<CapturedData>(destroyed, destruction_thread);
            cancelled = runtime.simulation_dispatcher().submit([data = std::move(data)] {});
            runtime.stop();
        };
        game.on_deinit = [&] { EXPECT_TRUE(destroyed); };
        runtime.run();
        EXPECT_EQ(destruction_thread, game.update_thread);
        try {
            cancelled.get();
            FAIL() << "Stopped simulation work must be cancelled";
        }
        catch (const std::future_error& error) {
            EXPECT_EQ(error.code(), std::make_error_code(std::future_errc::broken_promise));
        }
    }
}

TEST(simulation_requests, initialization_failure_cancels_before_game_cleanup_without_a_worker) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        std::future<void> cancelled;
        game.on_init = [&] {
            cancelled = runtime.simulation_dispatcher().submit([] {});
            throw std::runtime_error("initialization failed");
        };
        game.on_deinit = [&] {
            EXPECT_EQ(cancelled.wait_for(std::chrono::seconds{0}), std::future_status::ready);
        };
        EXPECT_THROW(runtime.run(), std::runtime_error);
        EXPECT_THROW(cancelled.get(), std::future_error);
        EXPECT_EQ(game.shutdowns, 1);
    }
}

TEST(simulation_requests, a_saved_endpoint_rejects_after_runtime_destruction) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    auto runtime = std::make_unique<CE::GFramework::GameRuntime>(*engine, game);
    auto endpoint = runtime->simulation_dispatcher().submission();
    game.on_tick = [&] { runtime->stop(); };
    runtime->run();
    runtime.reset();
    EXPECT_THROW(static_cast<void>(endpoint.submit([] {})), CE::Exceptions::failed_operation);
}
