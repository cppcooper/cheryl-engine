#include <gtest/gtest.h>

#include <assets/resources/resource-provider.h>
#include <assets/types/2d/graphic.h>
#include <core/controls/input-interface.h>
#include <core/display/display-system-interface.h>
#include <core/display/window-interface.h>
#include <core/engine/engine-context.h>
#include <core/engine/engine-context-internal.h>
#include <core/engine/worker-pool-internal.h>
#include <core/engine/event-delivery.h>
#include <core/game-framework/abstract-game.h>
#include <core/game-framework/game-runtime.h>
#include <core/game-framework/game-runtime-internal.h>
#include <assets/submission/draw2d.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <core/resources/asset-management/asset-loader.h>
#include <core/resources/asset-management/material-mgr.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/tileset-mgr.h>
#include <internals/exceptions.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <new>
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
        CE::FramebufferSize size_{320, 240};
        CE::Enum::window_mode mode_ = CE::Enum::window_mode::NORMAL;
        bool closed_ = false;
        mutable bool cursor_hidden_ = false;

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
    };

    /** Creates and activates MemoryWindow through the display interface. */
    class MemoryDisplay final : public CE::iDisplaySystem {
    public:
        std::function<void()> on_destroy;

    private:
        std::vector<CE::Monitor> monitors_{{1, 320, 240}};
        std::unique_ptr<MemoryWindow> window_;
        CE::iWindow* active_ = nullptr;

    public:
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
    };

    /** Emits one button transition per poll and records its attached window. */
    class MemoryInput final : public CE::Input::iInputSystem {
        CE::iWindow* window_ = nullptr;
        CE::Input::InputBindings bindings_;

    public:
        std::function<void()> on_poll;
        std::function<void()> on_destroy;
        std::function<void()> on_initialize;
        std::function<void()> on_deinitialize;
        bool default_press = true;
        int initializations = 0;
        int shutdowns = 0;

        ~MemoryInput() override {
            if (on_destroy)
                on_destroy();
        }

        void initialize(CE::iWindow& window) override {
            window_ = &window;
            ++initializations;
            if (on_initialize)
                on_initialize();
        }

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
            ++shutdowns;
            if (on_deinitialize)
                on_deinitialize();
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
        std::function<void()> on_quiesce;
        std::function<void(CE::RenderAPIs::RenderFrameWriter&)> on_prepare;
        int initializations = 0;
        int shutdowns = 0;
        int updates = 0;
        mutable int draws = 0;
        std::atomic<bool> pressed{false};
        std::thread::id update_thread;
        CE::FramebufferSize size{};
        std::vector<CE::Input::InputRecord> received_records;
        std::vector<double> simulation_deltas;
        std::vector<double> observed_intervals;
        std::vector<double> dropped_intervals;
        std::vector<CE::GFramework::UpdateKind> update_kinds;

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

        void quiesce() override {
            if (on_quiesce)
                on_quiesce();
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
            simulation_deltas.push_back(tick.delta_seconds);
            observed_intervals.push_back(tick.observed_seconds());
            dropped_intervals.push_back(tick.dropped_seconds);
            update_kinds.push_back(tick.update_kind);
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
            if (on_prepare) {
                on_prepare(frame);
                return;
            }
            glm::mat4 view{1.0f};
            view[3][0] = pressed.load() ? 1.0f : 0.0f;
            (void)frame.begin_pass(glm::mat4{1.0f}, view);
        }
    };

    class MemoryImage final : public CE::Assets::Image {
        CE::Assets::PixelSize size_;

    public:
        mutable std::vector<std::uint32_t> bound_units;

        explicit MemoryImage(CE::Assets::PixelSize size)
        : size_(size) {}

        [[nodiscard]] CE::Assets::PixelSize pixel_size() const override { return size_; }
        void bind(std::uint32_t unit) const override { bound_units.push_back(unit); }
    };

    /** Records independent geometry binding and draw ranges instead of GPU commands. */
    class MemoryGeometry final : public CE::Assets::Geometry2D {
        mutable std::size_t binds_ = 0;

    public:
        mutable std::size_t first_vertex = 0;
        mutable std::size_t drawn_vertices = 0;
        CE::Assets::PrimitiveTopology uploaded_topology = CE::Assets::PrimitiveTopology::Triangles;
        std::size_t uploaded_vertices = 0;

        [[nodiscard]] CE::Assets::VertexLayout2D vertex_layout() const noexcept override {
            return CE::Assets::VertexLayout2D::Position3UV2;
        }
        [[nodiscard]] CE::Assets::PrimitiveTopology topology() const noexcept override { return uploaded_topology; }
        [[nodiscard]] std::size_t vertex_count() const noexcept override { return uploaded_vertices; }
        void bind() const override { ++binds_; }

        void draw(std::size_t first, std::size_t count) const override {
            first_vertex = first;
            drawn_vertices = count;
        }

        [[nodiscard]] std::size_t bind_count() const { return binds_; }
    };

    class MemoryPipeline final : public CE::Assets::Pipeline {
    public:
        explicit MemoryPipeline(
            float intensity,
            CE::Assets::PrimitiveTopology topology = CE::Assets::PrimitiveTopology::Triangles,
            bool image = false
        ) : Pipeline(make_definition(intensity, topology, image)) {}

    private:
        static CE::Assets::PipelineDefinition make_definition(
            float intensity,
            CE::Assets::PrimitiveTopology topology,
            bool image
        ) {
            CE::Assets::PipelineDefinition result;
            result.program_sources = {"memory.vert", "memory.frag"};
            result.topology = topology;
            result.parameters = {{"intensity", CE::Assets::ParameterType::Float, true,
                CE::Assets::ParameterSemantic::Custom, intensity}};
            if (image)
                result.parameters.push_back({"image", CE::Assets::ParameterType::Sampler2D});
            return result;
        }
    };

    /** Records shader uses and camera matrices passed during drawing. */
    class MemoryShader final : public CE::Assets::Shader {
    public:
        CE::Assets::ShaderDraw last_draw;
        int raw_uniform_writes = 0;
        int uses = 0;
        glm::mat4 projection{0.0f};
        glm::mat4 view{0.0f};

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
    };

    /** Supplies in-memory assets and records the geometry uploaded by asset managers. */
    class MemoryProvider final : public CE::Assets::ResourceProvider {
    public:
        std::uint32_t uploaded_vertices = 0;
        std::thread::id resource_thread;
        int atlas_uploads = 0;
        int linked_programs = 0;
        int created_images = 0;
        std::function<std::shared_ptr<CE::Assets::Image>()> on_load_image;
        std::function<std::shared_ptr<CE::Assets::Geometry2D>(std::span<const CE::Vertex2D>, CE::Assets::PrimitiveTopology)>
            on_upload_geometry;
        CE::Assets::PrimitiveTopology uploaded_topology = CE::Assets::PrimitiveTopology::Triangles;
        std::vector<CE::Vertex2D> uploaded_geometry;
        std::shared_ptr<MemoryGeometry> geometry = std::make_shared<MemoryGeometry>();
        std::shared_ptr<MemoryShader> shader = std::make_shared<MemoryShader>();

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
            if (on_upload_geometry)
                return on_upload_geometry(vertices, topology);
            uploaded_vertices = vertices.size();
            uploaded_topology = topology;
            uploaded_geometry.assign(vertices.begin(), vertices.end());
            geometry = std::make_shared<MemoryGeometry>();
            geometry->uploaded_vertices = vertices.size();
            geometry->uploaded_topology = topology;
            return geometry;
        }

        [[nodiscard]] std::shared_ptr<CE::Assets::Shader> link_program(const std::vector<std::filesystem::path>&) override {
            ++linked_programs;
            return shader;
        }
    };

    /** Records frame handoff while the test supplies its own display and input. */
    class MemoryRenderer final : public CE::RenderAPIs::iRenderer {
    public:
        std::function<void()> on_render;
        std::function<void(const CE::RenderAPIs::RenderFrame&)> on_frame;
        std::function<void()> on_maintenance;
        int maintenance_calls = 0;
        std::thread::id maintenance_thread;
        std::function<void()> on_initialize;
        std::function<void()> on_deinitialize;
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

        void initialize() override {
            ++initializations;
            if (on_initialize)
                on_initialize();
        }

        void deinitialize() override {
            ++shutdowns;
            if (on_deinitialize)
                on_deinitialize();
        }
        void maintain_resources() override {
            ++maintenance_calls;
            maintenance_thread = std::this_thread::get_id();
            if (on_maintenance)
                on_maintenance();
        }
        void clear() override { ++clears; }

        void render(const CE::RenderAPIs::RenderFrame& frame) override {
            ++renders;
            last_pass_count = frame.passes().size();
            last_marked_pressed = !frame.passes().empty() && frame.passes().front().view[3][0] == 1.0f;
            render_thread = std::this_thread::get_id();
            if (on_frame)
                on_frame(frame);
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
    };

    class MemorySurface final : public CE::RenderAPIs::iPresentationSurface {
    public:
        void present() override { ++presents; }
        int presents = 0;
    };

    // Construct the same owned adapter graph as the GLFW factory, with no native graphics API.
    std::unique_ptr<CE::Engine::EngineContext> make_test_context(MemoryInput& input, MemoryRenderer*& renderer, MemorySurface*& surface,
        CE::Engine::ExecutionOptions execution = CE::Engine::ExecutionOptions{}) {
        auto display = std::make_unique<MemoryDisplay>();
        auto* window = display->create_window(display->primary_monitor(), CE::Enum::window_mode::NORMAL, 320, 240);
        display->activate_window(*window);
        auto presentation = std::make_unique<MemorySurface>();
        surface = presentation.get();
        auto rendering = std::make_unique<MemoryRenderer>();
        renderer = rendering.get();
        return std::make_unique<CE::Engine::EngineContext>(std::move(display), std::move(presentation), std::move(rendering),
            std::make_unique<MemoryProvider>(), input, std::move(execution));
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

TEST(runtime_adapter, partial_adapter_failure_settles_context_groups_and_preserves_the_startup_error) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        for (const bool fail_input : {false, true}) {
            SCOPED_TRACE(mode == CE::GFramework::RunMode::Sequential ? "sequential" : "concurrent");
            SCOPED_TRACE(fail_input ? "partial input" : "partial renderer");
            MemoryInput input;
            MemoryRenderer* renderer = nullptr;
            MemorySurface* surface = nullptr;
            auto engine = make_test_context(input, renderer, surface);
            auto group = engine->make_worker_group();
            auto owner = std::make_shared<int>(42);
            std::weak_ptr<int> capture = owner;
            auto result = group.submit([owner = std::move(owner)] { return *owner; });
            OneTickGame game(input);
            CE::GFramework::GameRuntime runtime(*engine, game, mode);
            const char* original = fail_input ? "Original input startup failure" : "Original renderer startup failure";
            if (fail_input) {
                input.on_initialize = [&] {
                    EXPECT_NE(input.attached_window(), nullptr);
                    throw std::runtime_error(original);
                };
            } else {
                renderer->on_initialize = [&] { throw std::runtime_error(original); };
            }
            input.on_deinitialize = [] { throw std::runtime_error("Later input cleanup failure"); };
            renderer->on_deinitialize = [] { throw std::runtime_error("Later renderer cleanup failure"); };
            try {
                runtime.run();
                ADD_FAILURE() << "The selected adapter must reject startup";
            } catch (const std::runtime_error& error) {
                EXPECT_STREQ(error.what(), original);
            }
            EXPECT_FALSE(group.status().accepting);
            ASSERT_EQ(result.wait_for(std::chrono::seconds{0}), std::future_status::ready);
            EXPECT_EQ(result.get(), 42);
            EXPECT_TRUE(capture.expired());
            EXPECT_EQ(input.initializations, fail_input ? 1 : 0);
            EXPECT_EQ(input.shutdowns, fail_input ? 1 : 0);
            EXPECT_EQ(input.attached_window(), nullptr);
            EXPECT_EQ(renderer->shutdowns, 1);
            EXPECT_EQ(renderer->maintenance_calls == 0, !fail_input);
            EXPECT_EQ(game.initializations, 0);
            EXPECT_EQ(game.shutdowns, 0);
            EXPECT_THROW((void)group.submit([] {}), CE::Exceptions::failed_operation);
        }
    }
}

TEST(runtime_adapter, missing_active_window_closes_workers_without_starting_any_adapter) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        SCOPED_TRACE(mode == CE::GFramework::RunMode::Sequential ? "sequential" : "concurrent");
        MemoryInput input;
        auto rendering = std::make_unique<MemoryRenderer>();
        auto* renderer = rendering.get();
        auto engine = std::make_unique<CE::Engine::EngineContext>(std::make_unique<MemoryDisplay>(),
            std::make_unique<MemorySurface>(), std::move(rendering), std::make_unique<MemoryProvider>(), input);
        auto group = engine->make_worker_group();
        auto owner = std::make_shared<int>(42);
        std::weak_ptr<int> capture = owner;
        auto result = group.submit([owner = std::move(owner)] { return *owner; });
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        try {
            runtime.run();
            ADD_FAILURE() << "An absent window must reject startup";
        } catch (const CE::Exceptions::failed_operation& error) {
            EXPECT_NE(std::string_view(error.what()).find("no active window"), std::string_view::npos);
        }
        EXPECT_FALSE(group.status().accepting);
        ASSERT_EQ(result.wait_for(std::chrono::seconds{0}), std::future_status::ready);
        EXPECT_EQ(result.get(), 42);
        EXPECT_TRUE(capture.expired());
        EXPECT_EQ(renderer->initializations, 0);
        EXPECT_EQ(renderer->shutdowns, 0);
        EXPECT_EQ(game.initializations, 0);
        EXPECT_EQ(game.shutdowns, 0);
        EXPECT_EQ(input.attached_window(), nullptr);
        EXPECT_THROW((void)engine->make_worker_group(), CE::Exceptions::failed_operation);
        EXPECT_THROW((void)group.submit([] {}), CE::Exceptions::failed_operation);
    }
}

TEST(runtime_adapter, stop_before_start_finishes_groups_and_keeps_mailbox_targets_closed) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        SCOPED_TRACE(mode == CE::GFramework::RunMode::Sequential ? "sequential" : "concurrent");
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        auto group = engine->make_worker_group();
        auto platform = engine->platform_dispatcher().submission();
        auto simulation = runtime.simulation_dispatcher().submission();
        auto owner = std::make_shared<int>(42);
        std::weak_ptr<int> capture = owner;
        auto result = group.submit([platform, owner = std::move(owner)] {
            EXPECT_THROW((void)platform.submit([](CE::Engine::EngineContext&) {}), CE::Exceptions::failed_operation);
            return *owner;
        });
        runtime.stop();
        EXPECT_NO_THROW(runtime.run());
        EXPECT_FALSE(group.status().accepting);
        ASSERT_EQ(result.wait_for(std::chrono::seconds{0}), std::future_status::ready);
        EXPECT_EQ(result.get(), 42);
        EXPECT_TRUE(capture.expired());
        EXPECT_EQ(renderer->initializations, 0);
        EXPECT_EQ(renderer->shutdowns, 0);
        EXPECT_EQ(game.initializations, 0);
        EXPECT_EQ(game.shutdowns, 0);
        EXPECT_THROW((void)platform.submit([](CE::Engine::EngineContext&) {}), CE::Exceptions::failed_operation);
        EXPECT_THROW((void)simulation.submit([] {}), CE::Exceptions::failed_operation);
        EXPECT_THROW((void)engine->make_worker_group(), CE::Exceptions::failed_operation);
        EXPECT_THROW(runtime.run(), CE::Exceptions::failed_operation);
    }
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
    CE::RenderAPIs::DrawStyle2D style;
    style.material = std::make_shared<CE::Assets::Material>(CE::Assets::MaterialDefinition{
        std::make_shared<MemoryPipeline>(1.0f, CE::Assets::PrimitiveTopology::TriangleStrip, true),
        {{"image", CE::Assets::ImageBinding{sprite->texture, 3}}}});
    const CE::Assets::SubmissionContext2D context{pass.semantics(), pass.parameters(), pass.constraints()};
    pass.add(CE::Assets::resolve_sprite(*sprite, 0, style, context));
    pass.add(CE::Assets::resolve_sprite(*sprite, 1, style, context));

    EXPECT_EQ(provider.uploaded_vertices, 8u);
    EXPECT_EQ(provider.uploaded_topology, CE::Assets::PrimitiveTopology::TriangleStrip);
    ASSERT_EQ(frame.passes().size(), 1u);
    ASSERT_EQ(frame.passes()[0].draws.size(), 2u);
    const auto& first = frame.passes()[0].draws[0];
    const auto& second = frame.passes()[0].draws[1];
    EXPECT_EQ(first.geometry, second.geometry);
    EXPECT_EQ(first.material, second.material);
    EXPECT_EQ(first.first_vertex, 0u);
    EXPECT_EQ(second.first_vertex, 4u);
    EXPECT_EQ(std::get<CE::Assets::ImageBinding>(first.parameters.at("image")).unit, 3u);
    EXPECT_EQ(provider.geometry->bind_count(), 0u);
    EXPECT_EQ(std::dynamic_pointer_cast<MemoryImage>(sprite->texture)->bound_units.size(), 0u);
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

    CE::RenderAPIs::DrawStyle2D style;
    style.material = std::make_shared<CE::Assets::Material>(CE::Assets::MaterialDefinition{
        std::make_shared<MemoryPipeline>(1.0f, CE::Assets::PrimitiveTopology::Triangles, true),
        {{"image", CE::Assets::ImageBinding{graphic.texture, 0}}}});
    const auto packet = CE::Assets::resolve_graphic(graphic, style, {});
    EXPECT_EQ(packet.first_vertex, 0u);
    EXPECT_EQ(packet.vertex_count, 6u);
    EXPECT_EQ(packet.geometry, graphic.geometry);
    EXPECT_EQ(std::get<CE::Assets::ImageBinding>(packet.parameters.at("image")).image, graphic.texture);
    const auto image = std::dynamic_pointer_cast<MemoryImage>(graphic.texture);
    ASSERT_TRUE(image);
    EXPECT_TRUE(image->bound_units.empty());
    EXPECT_EQ(provider.geometry->bind_count(), 0u);
    EXPECT_EQ(provider.shader->uses, 0);
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

TEST(resource_upload, geometry_failure_releases_transient_cpu_and_graphic_image_owners) {
    MemoryProvider provider;
    auto quad = std::make_shared<CE::Quad>();
    std::weak_ptr<CE::Quad> cpu_owner = quad;
    std::shared_ptr<CE::Vertex2D> vertices{quad, quad->vertices.data()};
    quad.reset();
    provider.on_upload_geometry = [&](auto borrowed, auto) -> std::shared_ptr<CE::Assets::Geometry2D> {
        EXPECT_FALSE(cpu_owner.expired());
        EXPECT_EQ(borrowed.size(), 6u);
        throw CE::Exceptions::bad_alloc(CE_HERE);
    };
    EXPECT_THROW((void)provider.upload_geometry(std::move(vertices), 6, CE::Assets::PrimitiveTopology::Triangles),
        CE::Exceptions::bad_alloc);
    EXPECT_TRUE(cpu_owner.expired());

    auto image = std::make_shared<MemoryImage>(CE::Assets::PixelSize{1, 1});
    std::weak_ptr<MemoryImage> image_owner = image;
    provider.on_upload_geometry = [](auto borrowed, auto) -> std::shared_ptr<CE::Assets::Geometry2D> {
        EXPECT_EQ(borrowed.size(), 6u);
        throw CE::Exceptions::bad_alloc(CE_HERE);
    };
    EXPECT_THROW((void)CE::Assets::Graphic::from_image(std::move(image), provider, {0.0f, 0.0f}), CE::Exceptions::bad_alloc);
    EXPECT_TRUE(image_owner.expired());
}

TEST(asset_preparation, partial_sprite_and_tileset_upload_preserves_metadata_and_retained_resources) {
    using namespace CE::Assets;
    for (const bool tilesets : {false, true}) {
        SCOPED_TRACE(tilesets);
        MemoryProvider provider;
        Loader loader("unused-prepared-root");
        const auto image_key = std::filesystem::path{"audit-category.png"};
        const auto make_prepared = [&](const std::vector<std::string>& names) {
            PreparedAssets prepared;
            prepared.images.push_back({image_key, {{1, 1}, {255, 255, 255, 255}}});
            AssetManifest manifest;
            manifest.name_space = "audit-category";
            for (const auto& name : names) {
                if (tilesets) {
                    TilesetDefinition definition{};
                    definition.name_space = manifest.name_space;
                    definition.name = name;
                    definition.texture = image_key;
                    definition.grid = {.frame = {1, 1}, .rows = 1, .columns = 1};
                    manifest.tilesets.push_back(std::move(definition));
                }
                else {
                    SpriteDefinition definition{};
                    definition.name_space = manifest.name_space;
                    definition.name = name;
                    definition.texture = image_key;
                    definition.grid = {.frame = {1, 1}, .rows = 1, .columns = 1};
                    manifest.sprites.push_back(std::move(definition));
                }
            }
            prepared.manifests.push_back(std::move(manifest));
            return prepared;
        };
        const auto geometry_for = [&](const std::string& name) -> std::shared_ptr<Geometry2D> {
            const auto key = "audit-category:" + name;
            if (tilesets) {
                const auto asset = TilesetMgr::get().get_asset(key);
                return asset ? asset->geometry : nullptr;
            }
            const auto asset = SpriteMgr::get().get_asset(key);
            return asset ? asset->geometry : nullptr;
        };
        int uploads = 0;
        int reject_at = 3;
        std::vector<std::weak_ptr<Geometry2D>> native_owners;
        provider.on_upload_geometry = [&](auto vertices, auto topology) -> std::shared_ptr<Geometry2D> {
            if (++uploads == reject_at)
                throw CE::Exceptions::bad_alloc(CE_HERE);
            auto geometry = std::make_shared<MemoryGeometry>();
            geometry->uploaded_vertices = static_cast<std::uint32_t>(vertices.size());
            geometry->uploaded_topology = topology;
            native_owners.push_back(geometry);
            return geometry;
        };
        loader.upload(make_prepared({"kept"}), provider);
        const auto previous = loader.manifests();
        auto retained = geometry_for("kept");
        ASSERT_TRUE(retained);
        EXPECT_THROW(loader.upload(make_prepared({"complete", "failed"}), provider), CE::Exceptions::bad_alloc);
        EXPECT_EQ(loader.manifests(), previous);
        EXPECT_EQ(geometry_for("kept"), retained);
        EXPECT_TRUE(geometry_for("complete"));
        EXPECT_FALSE(geometry_for("failed"));
        ASSERT_EQ(native_owners.size(), 2u);
        EXPECT_FALSE(native_owners[0].expired());
        EXPECT_FALSE(native_owners[1].expired());
        reject_at = 0;
        loader.upload(make_prepared({"complete", "failed"}), provider);
        EXPECT_NE(loader.manifests(), previous);
        EXPECT_EQ(previous->front().name_space, "audit-category");
        EXPECT_TRUE(geometry_for("failed"));
        ASSERT_EQ(native_owners.size(), 3u);
        if (tilesets)
            TilesetMgr::get().clear_assets();
        else
            SpriteMgr::get().clear_assets();
        EXPECT_FALSE(native_owners[0].expired());
        EXPECT_TRUE(native_owners[1].expired());
        EXPECT_TRUE(native_owners[2].expired());
        retained.reset();
        EXPECT_TRUE(native_owners[0].expired());
    }
}

TEST(platform_requests, a_saved_submission_endpoint_rejects_after_context_destruction) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    auto endpoint = engine->platform_dispatcher().submission();
    OneTickGame game(input);
    auto runtime = std::make_unique<CE::GFramework::GameRuntime>(*engine, game);
    game.on_tick = [&] { runtime->stop(); };
    runtime->run();
    runtime.reset();
    game.on_tick = {};
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

TEST(event_delivery, platform_and_simulation_targets_execute_on_their_runtime_owners) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        CE::SubSystems::EventBus bus;
        std::thread::id platform_thread;
        std::thread::id simulation_thread;
        const auto report = [](std::exception_ptr) { ADD_FAILURE() << "Unexpected event delivery failure"; };
        bus.register_listener("tick", [&](std::any) { platform_thread = std::this_thread::get_id(); },
            CE::Engine::platform_event_delivery(engine->platform_dispatcher().submission()), report);
        bus.register_listener("tick", [&](std::any) { simulation_thread = std::this_thread::get_id(); },
            CE::Engine::simulation_event_delivery(runtime.simulation_dispatcher().submission()), report);
        game.on_init = [&] { bus.dispatch("tick", 0); };
        game.on_tick = [&] { runtime.stop(); };
        runtime.run();
        EXPECT_EQ(platform_thread, std::this_thread::get_id());
        EXPECT_EQ(simulation_thread, game.update_thread);
        bus.close();
    }
}

TEST(execution_shutdown, accepted_worker_upload_can_finish_while_the_platform_is_stopping) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        auto group = engine->make_worker_group();
        auto platform = engine->platform_dispatcher().submission();
        std::future<int> result;
        std::promise<void> posted;
        bool quiesced = false;
        std::thread::id upload_thread;
        game.on_init = [&] {
            result = group.submit([&] {
                auto uploading = platform.submit([&](CE::Engine::EngineContext&) {
                    upload_thread = std::this_thread::get_id();
                    return 42;
                });
                posted.set_value();
                // A CPU worker may await platform completion; shutdown must pump.
                return uploading.get();
            });
            posted.get_future().wait();
            runtime.stop();
        };
        game.on_quiesce = [&] { quiesced = true; };
        game.on_deinit = [&] {
            EXPECT_TRUE(quiesced);
            EXPECT_EQ(result.wait_for(std::chrono::seconds{0}), std::future_status::ready);
        };
        runtime.run();
        EXPECT_EQ(result.get(), 42);
        EXPECT_EQ(upload_thread, std::this_thread::get_id());
        EXPECT_FALSE(group.status().accepting);
        EXPECT_THROW(static_cast<void>(engine->make_worker_group()), CE::Exceptions::failed_operation);
    }
}

TEST(execution_shutdown, asset_initialization_failure_preserves_pending_upload_and_simulation_cleanup) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        SCOPED_TRACE(mode == CE::GFramework::RunMode::Sequential ? "sequential" : "concurrent");
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        auto& provider = static_cast<MemoryProvider&>(engine->resources());
        OneTickGame game(input, true);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        auto group = engine->make_worker_group();
        auto platform = engine->platform_dispatcher().submission();
        auto simulation = runtime.simulation_dispatcher().submission();
        std::promise<void> posted;
        auto posted_future = posted.get_future();
        std::future<int> result;
        std::future<void> cancelled;
        std::weak_ptr<CE::Vertex2D> cpu_owner;
        int uploads = 0;
        int quiesces = 0;
        const auto platform_thread = std::this_thread::get_id();
        provider.on_upload_geometry = [](auto, auto) -> std::shared_ptr<CE::Assets::Geometry2D> {
            throw std::bad_alloc{};
        };
        game.on_init = [&] {
            cancelled = simulation.submit([] {});
            result = group.submit([&] {
                auto upload = platform.submit([&](CE::Engine::EngineContext& context) {
                    EXPECT_EQ(std::this_thread::get_id(), platform_thread);
                    const CE::Assets::DecodedImage pixels{CE::Assets::PixelSize{1, 1}, {255, 255, 255, 255}};
                    auto image = context.resources().create_image(pixels);
                    EXPECT_EQ(image->pixel_size().width, 1u);
                    ++uploads;
                    return 42;
                });
                posted.set_value();
                return upload.get();
            });
            if (posted_future.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
                throw std::runtime_error("Worker did not post its upload");
            auto vertices = std::shared_ptr<CE::Vertex2D>(new CE::Vertex2D[6]{}, std::default_delete<CE::Vertex2D[]>{});
            cpu_owner = vertices;
            (void)provider.upload_geometry(std::move(vertices), 6, CE::Assets::PrimitiveTopology::Triangles);
        };
        game.on_quiesce = [&] {
            ++quiesces;
            throw std::runtime_error("Later producer cleanup failed");
        };
        game.on_deinit = [&] {
            ASSERT_EQ(result.wait_for(std::chrono::seconds{0}), std::future_status::ready);
            EXPECT_EQ(result.get(), 42);
            EXPECT_EQ(cancelled.wait_for(std::chrono::seconds{0}), std::future_status::ready);
            EXPECT_THROW(cancelled.get(), std::future_error);
            EXPECT_TRUE(cpu_owner.expired());
        };
        EXPECT_THROW(runtime.run(), std::bad_alloc);
        EXPECT_EQ(uploads, 1);
        EXPECT_EQ(quiesces, 1);
        EXPECT_EQ(game.updates, 0);
        EXPECT_EQ(game.shutdowns, 1);
        EXPECT_EQ(renderer->shutdowns, 1);
        EXPECT_EQ(input.attached_window(), nullptr);
        EXPECT_EQ(input.routing().current()->target, 0u);
        EXPECT_FALSE(group.status().accepting);
        EXPECT_THROW((void)group.submit([] {}), CE::Exceptions::failed_operation);
        EXPECT_THROW((void)platform.submit([](CE::Engine::EngineContext&) {}), CE::Exceptions::failed_operation);
        EXPECT_THROW((void)simulation.submit([] {}), CE::Exceptions::failed_operation);
    }
}

TEST(execution_shutdown, failed_worker_upload_settles_before_game_cleanup_and_preserves_its_error) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        SCOPED_TRACE(mode == CE::GFramework::RunMode::Sequential ? "sequential" : "concurrent");
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        auto& provider = static_cast<MemoryProvider&>(engine->resources());
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        auto group = engine->make_worker_group();
        auto platform = engine->platform_dispatcher().submission();
        std::promise<void> posted;
        auto posted_future = posted.get_future();
        std::future<void> result;
        std::weak_ptr<CE::Vertex2D> cpu_owner;
        provider.on_upload_geometry = [](auto, auto) -> std::shared_ptr<CE::Assets::Geometry2D> {
            throw std::bad_alloc{};
        };
        game.on_init = [&] {
            auto vertices = std::shared_ptr<CE::Vertex2D>(new CE::Vertex2D[6]{}, std::default_delete<CE::Vertex2D[]>{});
            cpu_owner = vertices;
            result = group.submit([platform, vertices = std::move(vertices), &posted]() mutable {
                auto upload = platform.submit([vertices = std::move(vertices)](CE::Engine::EngineContext& context) mutable {
                    return context.resources().upload_geometry(std::move(vertices), 6, CE::Assets::PrimitiveTopology::Triangles);
                });
                posted.set_value();
                (void)upload.get();
            });
            if (posted_future.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
                throw std::runtime_error("Worker did not post its upload");
            runtime.stop();
        };
        game.on_deinit = [&] {
            ASSERT_EQ(result.wait_for(std::chrono::seconds{0}), std::future_status::ready);
            EXPECT_TRUE(cpu_owner.expired());
            result.get(); // The application's observation propagates the worker's original error.
        };
        renderer->on_deinitialize = [] { throw std::runtime_error("Later renderer cleanup failed"); };
        EXPECT_THROW(runtime.run(), std::bad_alloc);
        EXPECT_EQ(game.shutdowns, 1);
        EXPECT_EQ(renderer->shutdowns, 1);
        EXPECT_EQ(input.attached_window(), nullptr);
        EXPECT_TRUE(cpu_owner.expired());
        EXPECT_FALSE(group.status().accepting);
    }
}

TEST(execution_shutdown, policy_failed_preparation_settles_without_upload_or_injected_root_teardown) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        SCOPED_TRACE(mode == CE::GFramework::RunMode::Sequential ? "sequential" : "concurrent");
        CE::Engine::WorkerDetail::WorkerNativeAdapter native{
            true,
            [] { return std::vector<unsigned int>{2, 7}; },
            [](const std::vector<unsigned int>& mask) {
                if (mask == std::vector<unsigned int>{2})
                    throw CE::Exceptions::failed_operation(CE_HERE, "Controlled runtime affinity failure");
            },
            [](std::function<void()> work) { return std::thread(std::move(work)); }
        };
        std::shared_ptr<CE::Engine::WorkerPool> root{CE::Engine::WorkerDetail::WorkerPoolAccess::create(1, std::move(native))};
        auto unrelated = root->make_group();
        CE::Engine::ExecutionOptions execution;
        execution.shared_pool = root;
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface, execution);
        auto& provider = static_cast<MemoryProvider&>(engine->resources());
        CE::Engine::WorkerGroupOptions policy;
        policy.cpu.cpus = {2};
        policy.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
        auto group = engine->make_worker_group(policy);
        auto platform = engine->platform_dispatcher().submission();
        auto pixels = std::make_shared<CE::Assets::DecodedImage>(
            CE::Assets::DecodedImage{CE::Assets::PixelSize{1, 1}, {255, 255, 255, 255}}
        );
        std::weak_ptr<CE::Assets::DecodedImage> capture = pixels;
        std::atomic<int> callbacks = 0;
        std::future<std::shared_ptr<CE::Assets::Image>> result;
        OneTickGame game(input);
        auto runtime = std::make_unique<CE::GFramework::GameRuntime>(*engine, game, mode);
        game.on_init = [&] {
            result = group.submit([platform, pixels = std::move(pixels), &callbacks] {
                ++callbacks;
                auto upload = platform.submit([pixels](CE::Engine::EngineContext& context) {
                    return context.resources().create_image(*pixels);
                });
                return upload.get();
            });
            runtime->stop();
        };
        game.on_deinit = [&] {
            ASSERT_EQ(result.wait_for(std::chrono::seconds{0}), std::future_status::ready);
            EXPECT_THROW((void)result.get(), CE::Exceptions::failed_operation);
            EXPECT_TRUE(capture.expired());
        };
        EXPECT_NO_THROW(runtime->run());
        EXPECT_EQ(callbacks.load(), 0);
        EXPECT_EQ(provider.created_images, 0);
        EXPECT_EQ(group.status().policy_failures, 1u);
        EXPECT_FALSE(group.status().accepting);
        EXPECT_TRUE(unrelated.status().accepting);
        EXPECT_EQ(unrelated.submit([] { return 17; }).get(), 17);
        runtime.reset();
        game.on_init = {};
        game.on_deinit = {};
        engine.reset();
        EXPECT_TRUE(unrelated.status().accepting);
    }
}

TEST(execution_shutdown, owned_root_startup_rollback_settles_queued_callbacks_before_adapter_cleanup) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        SCOPED_TRACE(mode == CE::GFramework::RunMode::Sequential ? "sequential" : "concurrent");
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        CE::Engine::ExecutionOptions execution;
        execution.worker_count = 2;
        auto engine = make_test_context(input, renderer, surface, execution);
        std::promise<void> entered;
        auto started = entered.get_future().share();
        std::atomic<int> exited{0};
        int attempts = 0;
        std::weak_ptr<int> native_capture;
        CE::Engine::ContextDetail::EngineContextAccess::set_owned_worker_factory(*engine, [&](const std::size_t count) {
            auto owner = std::make_shared<int>(42);
            native_capture = owner;
            CE::Engine::WorkerDetail::WorkerNativeAdapter native;
            native.start_thread = [owner = std::move(owner), &attempts, &entered, started, &exited](std::function<void()> work) {
                if (++attempts == 2) {
                    if (started.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
                        throw std::runtime_error("First owned worker did not enter");
                    throw std::runtime_error("Controlled owned-root startup failure");
                }
                return std::thread([work = std::move(work), &entered, &exited] {
                    entered.set_value();
                    work();
                    ++exited;
                });
            };
            return CE::Engine::WorkerDetail::WorkerPoolAccess::create(count, std::move(native));
        });
        OneTickGame game(input);
        CE::GFramework::GameRuntime runtime(*engine, game, mode);
        auto platform = engine->platform_dispatcher().submission();
        auto simulation = runtime.simulation_dispatcher().submission();
        std::future<void> cancelled_platform;
        std::future<void> cancelled_simulation;
        std::weak_ptr<int> platform_capture;
        std::weak_ptr<int> simulation_capture;
        int callbacks = 0;
        game.on_init = [&] {
            auto first = std::make_shared<int>(1);
            auto second = std::make_shared<int>(2);
            platform_capture = first;
            simulation_capture = second;
            cancelled_platform = platform.submit([owner = std::move(first), &callbacks](CE::Engine::EngineContext&) { ++callbacks; });
            cancelled_simulation = simulation.submit([owner = std::move(second), &callbacks] { ++callbacks; });
            static_cast<void>(engine->make_worker_group());
        };
        game.on_deinit = [&] {
            EXPECT_EQ(cancelled_platform.wait_for(std::chrono::seconds{0}), std::future_status::ready);
            EXPECT_EQ(cancelled_simulation.wait_for(std::chrono::seconds{0}), std::future_status::ready);
            EXPECT_TRUE(platform_capture.expired());
            EXPECT_TRUE(simulation_capture.expired());
            EXPECT_TRUE(native_capture.expired());
            EXPECT_EQ(exited.load(), 1);
        };
        renderer->on_deinitialize = [] { throw std::runtime_error("Later renderer cleanup failed"); };
        try {
            runtime.run();
            FAIL() << "Owned pool startup must fail";
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(std::string_view(error.what()), "Controlled owned-root startup failure");
        }
        EXPECT_EQ(attempts, 2);
        EXPECT_EQ(callbacks, 0);
        EXPECT_EQ(game.shutdowns, 1);
        EXPECT_EQ(renderer->shutdowns, 1);
        EXPECT_EQ(input.attached_window(), nullptr);
        EXPECT_THROW(cancelled_platform.get(), CE::Exceptions::failed_operation);
        EXPECT_THROW(cancelled_simulation.get(), CE::Exceptions::failed_operation);
        EXPECT_THROW((void)engine->make_worker_group(), CE::Exceptions::failed_operation);
        EXPECT_THROW((void)platform.submit([](CE::Engine::EngineContext&) {}), CE::Exceptions::failed_operation);
        EXPECT_THROW((void)simulation.submit([] {}), CE::Exceptions::failed_operation);
    }
}

TEST(execution_shutdown, simulation_thread_start_failure_settles_worker_upload_and_unbound_simulation) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game, CE::GFramework::RunMode::Concurrent);
    auto group = engine->make_worker_group();
    auto platform = engine->platform_dispatcher().submission();
    auto simulation = runtime.simulation_dispatcher().submission();
    std::promise<void> worker_entered;
    auto entered = worker_entered.get_future();
    std::promise<void> release_upload;
    auto upload_gate = release_upload.get_future().share();
    std::future<int> uploaded;
    std::future<void> cancelled;
    std::weak_ptr<int> simulation_capture;
    std::weak_ptr<CE::Assets::DecodedImage> cpu_capture;
    int simulation_callbacks = 0;
    int starts = 0;
    std::thread::id upload_thread;
    CE::GFramework::RuntimeDetail::GameRuntimeAccess::set_simulation_thread_factory(
        runtime, [&](std::function<void()>) -> std::thread {
            ++starts;
            release_upload.set_value();
            throw std::runtime_error("Controlled simulation thread-start failure");
        }
    );
    game.on_init = [&] {
        auto owner = std::make_shared<int>(42);
        simulation_capture = owner;
        cancelled = simulation.submit([owner = std::move(owner), &simulation_callbacks] { ++simulation_callbacks; });
        auto pixels = std::make_shared<CE::Assets::DecodedImage>(
            CE::Assets::DecodedImage{CE::Assets::PixelSize{1, 1}, {255, 255, 255, 255}}
        );
        cpu_capture = pixels;
        uploaded = group.submit([pixels = std::move(pixels), platform, upload_gate, &worker_entered, &upload_thread] {
            worker_entered.set_value();
            if (upload_gate.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
                throw std::runtime_error("Simulation startup did not release upload");
            auto completion = platform.submit([pixels, &upload_thread](CE::Engine::EngineContext& context) {
                upload_thread = std::this_thread::get_id();
                static_cast<void>(context.resources().create_image(*pixels));
                return 42;
            });
            return completion.get();
        });
        if (entered.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
            throw std::runtime_error("Preparation worker did not enter");
    };
    game.on_quiesce = [] { throw std::runtime_error("Later quiesce failure"); };
    game.on_deinit = [&] {
        EXPECT_EQ(uploaded.wait_for(std::chrono::seconds{0}), std::future_status::ready);
        EXPECT_EQ(cancelled.wait_for(std::chrono::seconds{0}), std::future_status::ready);
        EXPECT_TRUE(cpu_capture.expired());
        EXPECT_TRUE(simulation_capture.expired());
    };
    renderer->on_deinitialize = [] { throw std::runtime_error("Later renderer cleanup failure"); };
    try {
        runtime.run();
        FAIL() << "Simulation thread startup must fail";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string_view(error.what()), "Controlled simulation thread-start failure");
    }
    EXPECT_EQ(starts, 1);
    EXPECT_EQ(uploaded.get(), 42);
    EXPECT_THROW(cancelled.get(), CE::Exceptions::failed_operation);
    EXPECT_EQ(simulation_callbacks, 0);
    EXPECT_EQ(upload_thread, std::this_thread::get_id());
    EXPECT_EQ(game.updates, 0);
    EXPECT_EQ(game.shutdowns, 1);
    EXPECT_EQ(input.attached_window(), nullptr);
    EXPECT_FALSE(group.status().accepting);
    EXPECT_THROW((void)platform.submit([](CE::Engine::EngineContext&) {}), CE::Exceptions::failed_operation);
    EXPECT_THROW((void)simulation.submit([] {}), CE::Exceptions::failed_operation);
}

TEST(execution_shutdown, an_injected_root_keeps_unrelated_application_groups_available) {
    auto root = std::make_shared<CE::Engine::WorkerPool>(2);
    auto unrelated = root->make_group();
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    CE::Engine::ExecutionOptions execution;
    execution.shared_pool = root;
    auto engine = make_test_context(input, renderer, surface, execution);
    auto game_group = engine->make_worker_group();
    OneTickGame game(input);
    auto runtime = std::make_unique<CE::GFramework::GameRuntime>(*engine, game);
    game.on_tick = [&] { runtime->stop(); };
    runtime->run();
    EXPECT_FALSE(game_group.status().accepting);
    EXPECT_TRUE(unrelated.status().accepting);
    EXPECT_EQ(unrelated.submit([] { return 17; }).get(), 17);
    runtime.reset();
    game.on_tick = {};
    engine.reset();
    EXPECT_TRUE(unrelated.status().accepting);
}

TEST(execution_shutdown, quiesce_failure_does_not_replace_initialization_failure) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::GameRuntime runtime(*engine, game);
    game.on_init = [] { throw std::runtime_error("original initialization failure"); };
    game.on_quiesce = [] { throw std::runtime_error("later quiesce failure"); };
    try {
        runtime.run();
        FAIL() << "Initialization must fail";
    }
    catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string_view(error.what()), "original initialization failure");
    }
    EXPECT_EQ(game.shutdowns, 1);
}

TEST(simulation_timing, both_runtime_modes_use_the_configured_fixed_delta) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::SimulationTimingOptions timing;
        timing.mode = CE::GFramework::SimulationMode::Fixed;
        timing.fixed_step = std::chrono::milliseconds{20};
        CE::GFramework::GameRuntime runtime(*engine, game, mode, CE::Input::PollingOptions{}, timing);
        game.on_tick = [&] {
            if (game.updates == 2)
                runtime.stop();
        };
        runtime.run();
        ASSERT_EQ(game.simulation_deltas.size(), 2u);
        for (std::size_t i = 0; i < game.simulation_deltas.size(); ++i) {
            EXPECT_DOUBLE_EQ(game.simulation_deltas[i], 0.020);
            EXPECT_EQ(game.update_kinds[i], CE::GFramework::UpdateKind::Fixed);
            EXPECT_GE(game.observed_intervals[i], 0.0);
        }
    }
}

TEST(simulation_timing, a_slow_update_triggers_capped_hybrid_recovery_in_both_modes) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::SimulationTimingOptions timing;
        timing.mode = CE::GFramework::SimulationMode::Fixed;
        timing.fixed_step = std::chrono::milliseconds{20};
        timing.recovery = CE::GFramework::LagRecovery::VariableCatchUp;
        timing.fixed_updates_before_recovery = 1;
        timing.recovery_cap = std::chrono::milliseconds{25};
        CE::GFramework::GameRuntime runtime(*engine, game, mode, CE::Input::PollingOptions{}, timing);
        game.on_tick = [&] {
            if (game.updates == 1)
                std::this_thread::sleep_for(std::chrono::milliseconds{80});
            if (game.update_kinds.back() == CE::GFramework::UpdateKind::VariableCatchUp)
                runtime.stop();
        };
        runtime.run();
        ASSERT_GE(game.update_kinds.size(), 2u);
        EXPECT_EQ(game.update_kinds.back(), CE::GFramework::UpdateKind::VariableCatchUp);
        EXPECT_DOUBLE_EQ(game.simulation_deltas.back(), 0.025);
        EXPECT_GT(game.dropped_intervals.back(), 0.0);
        EXPECT_EQ(game.update_kinds[game.update_kinds.size() - 2], CE::GFramework::UpdateKind::Fixed);
        EXPECT_DOUBLE_EQ(game.dropped_intervals[game.dropped_intervals.size() - 2], 0.0);
        EXPECT_LT(game.draws, game.updates);
        EXPECT_EQ(renderer->shutdowns, 1);
    }
}

TEST(simulation_timing, invalid_timing_is_rejected_before_any_adapter_initializes) {
    MemoryInput input;
    MemoryRenderer* renderer = nullptr;
    MemorySurface* surface = nullptr;
    auto engine = make_test_context(input, renderer, surface);
    OneTickGame game(input);
    CE::GFramework::SimulationTimingOptions timing;
    timing.fixed_step = std::chrono::milliseconds{0};
    EXPECT_THROW((void)CE::GFramework::GameRuntime(*engine, game, CE::GFramework::RunMode::Sequential,
        CE::Input::PollingOptions{}, timing), CE::Exceptions::invalid_args);
    EXPECT_EQ(renderer->initializations, 0);
    EXPECT_EQ(game.initializations, 0);
    EXPECT_EQ(input.attached_window(), nullptr);
}

TEST(resource_maintenance, both_modes_service_retirement_before_the_first_frame_with_a_full_input_backlog) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        int polls = 0;
        input.on_poll = [&] { ++polls; };
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::SimulationTimingOptions timing;
        timing.variable_interval = std::chrono::hours{1};
        CE::GFramework::GameRuntime runtime(*engine, game, mode, CE::Input::PollingOptions{}, timing);
        renderer->on_maintenance = [&] {
            if (renderer->maintenance_calls == 2) {
                // Lockstep capacity is full, and no simulation deadline/frame is
                // available to wake the platform. The bounded maintenance wait must.
                EXPECT_EQ(polls, 1);
                EXPECT_EQ(renderer->renders, 0);
                runtime.stop();
            }
        };
        runtime.run();
        EXPECT_GE(renderer->maintenance_calls, 2);
        EXPECT_EQ(renderer->maintenance_thread, std::this_thread::get_id());
        EXPECT_EQ(game.updates, 0);
        EXPECT_EQ(game.draws, 0);
        EXPECT_EQ(surface->presents, 0);
    }
}

TEST(resource_maintenance, a_maintenance_failure_preserves_its_error_through_cleanup_in_both_modes) {
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        MemoryInput input;
        MemoryRenderer* renderer = nullptr;
        MemorySurface* surface = nullptr;
        auto engine = make_test_context(input, renderer, surface);
        OneTickGame game(input);
        CE::GFramework::SimulationTimingOptions timing;
        timing.variable_interval = std::chrono::hours{1};
        CE::GFramework::GameRuntime runtime(*engine, game, mode, CE::Input::PollingOptions{}, timing);
        renderer->on_maintenance = [] { throw std::runtime_error("retirement maintenance failed"); };
        game.on_deinit = [] { throw std::runtime_error("later game cleanup failed"); };
        try {
            runtime.run();
            FAIL() << "Maintenance must fail";
        } catch (const std::runtime_error& error) {
            EXPECT_STREQ(error.what(), "retirement maintenance failed");
        }
        EXPECT_EQ(game.shutdowns, 1);
        EXPECT_EQ(renderer->shutdowns, 1);
        EXPECT_EQ(input.attached_window(), nullptr);
    }
}

TEST(asset_cache, strong_residency_survives_unused_handles_and_explicit_clear_preserves_external_owners) {
    auto& textures = CE::Assets::TextureMgr::get();
    auto first = std::make_unique<MemoryProvider>();
    MemoryProvider second;
    std::weak_ptr<CE::Assets::Image> observed;
    bool destroyed = false;
    first->on_load_image = [&] {
        auto image = std::shared_ptr<MemoryImage>(new MemoryImage({8, 16}), [&](MemoryImage* value) {
            destroyed = true;
            delete value;
        });
        observed = image;
        return image;
    };
    textures.load_assets({"resident.png"}, *first);
    EXPECT_FALSE(observed.expired()); // The cache alone retains an unused asset.
    EXPECT_FALSE(destroyed);
    EXPECT_THROW(textures.load_assets({"other-domain.png"}, second), CE::Exceptions::failed_operation);
    auto retained = textures.get_asset("resident.png");
    textures.clear_assets();
    EXPECT_EQ(textures.size(), 0u);
    EXPECT_FALSE(destroyed);
    ASSERT_TRUE(retained);
    EXPECT_EQ(retained->pixel_size().height, 16u);

    first.reset(); // Releases the old global domain, not this independent logical owner.
    EXPECT_FALSE(destroyed);
    textures.load_assets({"new-domain.png"}, second);
    EXPECT_TRUE(CE::Assets::AssetCacheContext::is_bound_to(second));
    EXPECT_EQ(retained->pixel_size().width, 8u);
    retained.reset();
    EXPECT_TRUE(destroyed);
    EXPECT_TRUE(observed.expired());
}

TEST(material_cache, successful_recipe_reload_retains_old_generations_and_failure_preserves_the_current_one) {
    auto& materials = CE::Assets::MaterialMgr::get();
    auto provider = std::make_unique<MemoryProvider>();
    MemoryProvider next_provider;
    int builds = 0;
    float intensity = 1.0f;
    const CE::Assets::MaterialMgr::Builder build = [&](CE::Assets::ResourceProvider& owner) {
        EXPECT_EQ(&owner, provider.get());
        ++builds;
        auto pipeline = std::make_shared<MemoryPipeline>(intensity);
        return std::make_shared<CE::Assets::Material>(CE::Assets::MaterialDefinition{pipeline, {}});
    };
    materials.load_material("effect", *provider, build);
    const auto old = materials.get_asset("effect");
    ASSERT_TRUE(old);
    intensity = 2.0f;
    materials.load_material("effect", *provider, build);
    EXPECT_EQ(builds, 1);
    materials.reload_material("effect", *provider, build);
    const auto current = materials.get_asset("effect");
    ASSERT_TRUE(current);
    EXPECT_NE(old.get(), current.get());
    EXPECT_FLOAT_EQ(std::get<float>(old->resolve({}, {}, {}, {}).at("intensity")), 1.0f);
    EXPECT_FLOAT_EQ(std::get<float>(current->resolve({}, {}, {}, {}).at("intensity")), 2.0f);

    EXPECT_THROW(materials.reload_material("effect", *provider,
        [](CE::Assets::ResourceProvider&) -> std::shared_ptr<const CE::Assets::Material> {
            throw std::runtime_error("candidate linking failed");
        }), std::runtime_error);
    EXPECT_THROW(materials.reload_material("effect", *provider,
        [](CE::Assets::ResourceProvider&) { return std::shared_ptr<const CE::Assets::Material>{}; }), CE::Exceptions::failed_operation);
    EXPECT_EQ(materials.get_asset("effect"), current);
    EXPECT_THROW(materials.load_material("effect", next_provider, build), CE::Exceptions::failed_operation);
    provider.reset();
    EXPECT_EQ(materials.size(), 0u);
    EXPECT_FLOAT_EQ(std::get<float>(old->resolve({}, {}, {}, {}).at("intensity")), 1.0f);
    materials.load_material("new-effect", next_provider, [](CE::Assets::ResourceProvider&) {
        return std::make_shared<CE::Assets::Material>(CE::Assets::MaterialDefinition{std::make_shared<MemoryPipeline>(3.0f), {}});
    });
    EXPECT_TRUE(CE::Assets::AssetCacheContext::is_bound_to(next_provider));
}

TEST(material_cache, recipe_reload_rejects_a_foreign_loading_thread_before_invoking_the_builder) {
    auto& materials = CE::Assets::MaterialMgr::get();
    MemoryProvider provider;
    int builds = 0;
    const CE::Assets::MaterialMgr::Builder build = [&](CE::Assets::ResourceProvider&) {
        ++builds;
        return std::make_shared<CE::Assets::Material>(CE::Assets::MaterialDefinition{std::make_shared<MemoryPipeline>(1.0f), {}});
    };
    materials.load_material("effect", provider, build);
    const auto current = materials.get_asset("effect");
    auto rejected = std::async(std::launch::async, [&] {
        EXPECT_THROW(materials.reload_material("effect", provider, build), CE::Exceptions::failed_operation);
    });
    rejected.get();
    EXPECT_EQ(builds, 1);
    EXPECT_EQ(materials.get_asset("effect"), current);
}

TEST(frame_lifetime, reload_and_preparation_or_render_failure_release_packet_resources_before_game_cleanup) {
    enum class Failure { None, Preparation, Rendering };
    for (const auto mode : {CE::GFramework::RunMode::Sequential, CE::GFramework::RunMode::Concurrent}) {
        for (const auto failure : {Failure::None, Failure::Preparation, Failure::Rendering}) {
            SCOPED_TRACE(mode == CE::GFramework::RunMode::Sequential ? "sequential" : "concurrent");
            SCOPED_TRACE(failure == Failure::Preparation ? "preparation failure"
                : failure == Failure::Rendering ? "render failure" : "successful reload");
            MemoryInput input;
            MemoryRenderer* renderer = nullptr;
            MemorySurface* surface = nullptr;
            auto engine = make_test_context(input, renderer, surface);
            OneTickGame game(input);
            CE::GFramework::GameRuntime runtime(*engine, game, mode);
            auto& materials = CE::Assets::MaterialMgr::get();
            const auto platform = std::this_thread::get_id();
            std::thread::id image_release_thread;
            std::shared_ptr<CE::Assets::Geometry2D> simulation_geometry;
            std::shared_ptr<const CE::Assets::Material> simulation_material;
            std::weak_ptr<const CE::Assets::Geometry2D> retained_geometry;
            std::weak_ptr<const CE::Assets::Material> retained_material;
            std::weak_ptr<const CE::Assets::Pipeline> retained_pipeline;
            std::weak_ptr<const CE::Assets::Image> retained_image;
            std::promise<void> rendered;
            auto first_frame = rendered.get_future().share();

            game.on_init = [&] {
                auto geometry = std::make_shared<MemoryGeometry>();
                geometry->uploaded_vertices = 6;
                simulation_geometry = geometry;
                retained_geometry = geometry;
                auto image = std::shared_ptr<MemoryImage>(new MemoryImage({1, 1}), [&](MemoryImage* resource) {
                    image_release_thread = std::this_thread::get_id();
                    delete resource;
                });
                retained_image = image;
                materials.load_material("frame-retention", engine->resources(), [image](CE::Assets::ResourceProvider&) {
                    return std::make_shared<CE::Assets::Material>(CE::Assets::MaterialDefinition{
                        std::make_shared<MemoryPipeline>(1.0f, CE::Assets::PrimitiveTopology::Triangles, true),
                        {{"image", CE::Assets::ImageBinding{image, 1}}}});
                });
                simulation_material = materials.get_asset("frame-retention");
                retained_material = simulation_material;
                retained_pipeline = simulation_material->definition().pipeline;
            };
            game.on_tick = [&] {
                // Hold a later concurrent tick so it cannot supersede the first
                // complete packet frame before the recording renderer inspects it.
                if (game.updates > 1)
                    first_frame.wait();
            };
            game.on_prepare = [&](CE::RenderAPIs::RenderFrameWriter& writer) {
                auto pass = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
                CE::RenderAPIs::DrawStyle2D style;
                style.material = std::move(simulation_material);
                pass.add(CE::RenderAPIs::resolve_draw_packet(std::move(simulation_geometry), 0, 6,
                    style, pass.semantics(), pass.parameters(), pass.constraints()));
                // A concurrent slot remains Writing here; cleanup still owns
                // its partially prepared packet and must recycle it on platform.
                if (failure == Failure::Preparation)
                    throw std::runtime_error("frame preparation failed");
            };
            renderer->on_frame = [&](const CE::RenderAPIs::RenderFrame& frame) {
                runtime.stop();
                rendered.set_value(); // Always release a held tick before assertions/failure.
                ASSERT_EQ(frame.passes().size(), 1u);
                ASSERT_EQ(frame.passes()[0].draws.size(), 1u);
                const auto& packet = frame.passes()[0].draws[0];
                EXPECT_EQ(packet.material, retained_material.lock());
                EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("intensity")), 1.0f);
                materials.reload_material("frame-retention", engine->resources(), [](CE::Assets::ResourceProvider&) {
                    return std::make_shared<CE::Assets::Material>(CE::Assets::MaterialDefinition{
                        std::make_shared<MemoryPipeline>(2.0f), {}});
                });
                EXPECT_NE(packet.material, materials.get_asset("frame-retention"));
                EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("intensity")), 1.0f);
                materials.clear_assets();
                EXPECT_FALSE(retained_material.expired());
                EXPECT_FALSE(retained_pipeline.expired());
                EXPECT_FALSE(retained_geometry.expired());
                EXPECT_FALSE(retained_image.expired());
                if (failure == Failure::Rendering)
                    throw std::runtime_error("frame render failed");
            };
            game.on_quiesce = [&] { materials.clear_assets(); };
            game.on_deinit = [&] {
                EXPECT_TRUE(retained_material.expired());
                EXPECT_TRUE(retained_pipeline.expired());
                EXPECT_TRUE(retained_geometry.expired());
                EXPECT_TRUE(retained_image.expired());
                EXPECT_EQ(image_release_thread, platform);
                if (failure != Failure::None)
                    throw std::runtime_error("later game cleanup failed");
            };
            if (failure == Failure::None)
                EXPECT_NO_THROW(runtime.run());
            else {
                try {
                    runtime.run();
                    FAIL() << "The selected frame stage must fail";
                } catch (const std::runtime_error& error) {
                    EXPECT_EQ(std::string_view(error.what()), failure == Failure::Preparation
                        ? "frame preparation failed" : "frame render failed");
                }
            }
            EXPECT_EQ(game.shutdowns, 1);
            EXPECT_EQ(renderer->shutdowns, 1);
            EXPECT_EQ(image_release_thread, platform);
        }
    }
}
