#include <text/text-updater.h>

#include <assets/resources/resource-provider.h>
#include <assets/submission/draw2d.h>
#include <core/controls/input-interface.h>
#include <core/display/display-system-interface.h>
#include <core/engine/engine-context.h>
#include <core/game-framework/abstract-game.h>
#include <core/game-framework/game-runtime.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <internals/exceptions.h>
#include <testing/material-builder.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error Text updates must compile without native or graphics SDK headers.
#endif

namespace {
    using namespace CE::Text::Rendering;
    using Mode = CE::Runtime::RunMode;

    // Hang guard only: constructing failure diagnostics can resolve Debug symbols.
    constexpr auto timeout = std::chrono::seconds{30};

    struct Geometry final : CE::Rendering::Geometry::Buffer2D {
        std::size_t count;
        explicit Geometry(const std::size_t count)
        : count(count) {}
        CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override {
            return CE::Rendering::Geometry::Layout2D::Position3UV2;
        }
        CE::Rendering::Geometry::Topology topology() const noexcept override { return CE::Rendering::Geometry::Topology::Triangles; }
        std::size_t vertex_count() const noexcept override { return count; }
        void bind() const override { throw std::logic_error("Text update must not draw"); }
        void draw(std::size_t, std::size_t) const override { throw std::logic_error("Text update must not draw"); }
    };

    struct Image final : CE::Rendering::Resources::Image {
        CE::Rendering::Geometry::PixelSize size;
        explicit Image(const CE::Rendering::Geometry::PixelSize size)
        : size(size) {}
        CE::Rendering::Geometry::PixelSize pixel_size() const override { return size; }
        void bind(std::uint32_t) const override { throw std::logic_error("Text update must not bind"); }
    };

    struct Provider final : CE::Rendering::Resources::Provider {
        CE::Testing::MaterialBuilder builder = CE::Testing::ordinary_materials();
        std::atomic<int> uploads = 0;
        std::atomic<int> attempts = 0;
        std::atomic<bool> fail = false;
        const std::thread::id owner = std::this_thread::get_id();

        CE::Assets::Shaders::Builder* shader_asset_builder() noexcept override { return &builder; }
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded&) override { return {}; }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override { return {}; }
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D> vertices, CE::Rendering::Geometry::Topology) override {
            EXPECT_EQ(std::this_thread::get_id(), owner);
            return std::make_shared<Geometry>(vertices.size());
        }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char>, const CE::Rendering::Geometry::PixelSize size) override {
            EXPECT_EQ(std::this_thread::get_id(), owner);
            ++attempts;
            if (fail)
                throw std::runtime_error("Injected atlas failure");
            ++uploads;
            return std::make_shared<Image>(size);
        }
    };

    struct Window final : CE::Platform::Window {
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout;
        CE::Platform::Dimensions<int> logical_size() const override { return {64, 64}; }
        CE::Platform::FramebufferSize framebuffer_size() const override { return {64, 64}; }
        CE::Platform::WindowMode mode() const override { return CE::Platform::WindowMode::NORMAL; }
        bool should_close() const override { return std::chrono::steady_clock::now() >= deadline; }
        void resize(int, int) override {}
        void set_mode(CE::Platform::WindowMode) override {}
        void hide_cursor(bool) const override {}
    };

    struct Display final : CE::Platform::DisplaySystem {
        std::vector<CE::Platform::Monitor> inventory{{1, 64, 64}};
        mutable Window window;
        const std::vector<CE::Platform::Monitor>& monitors() const override { return inventory; }
        int monitor_count() const override { return 1; }
        const CE::Platform::Monitor& primary_monitor() const override { return inventory.front(); }
        CE::Platform::Window* active_window() const override { return &window; }
        std::pair<float, float> content_scale(const CE::Platform::Monitor&) const override { return {1, 1}; }
        CE::Platform::Window* create_window(const CE::Platform::Monitor&, CE::Platform::WindowMode, int, int) override { return &window; }
        void activate_window(CE::Platform::Window&) override {}
    };

    struct Input final : CE::Input::System {
        CE::Input::Actions::Bindings actions;
        void initialize(CE::Platform::Window&) override {}
        void deinitialize() override {}
        void poll() override { static_cast<void>(actions.publish_actions()); }
        CE::Input::Actions::Bindings& bindings() override { return actions; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
    };

    struct Surface final : CE::Rendering::PresentationSurface {
        void present() override {}
    };

    struct Renderer final : CE::Rendering::Renderer {
        void initialize() override {}
        void deinitialize() override {}
        void maintain_resources() override {}
        void render(const CE::Rendering::Frame&) override {}
        void clear() override {}
        void set_viewport(CE::Platform::FramebufferSize) override {}
        void set_depth_test(bool) override {}
        void set_clear_colour(float, float, float, float) override {}
        void set_camera_matrices(const glm::mat4&, const glm::mat4&) override {}
    };

    struct Host {
        Provider* provider;
        CE::Runtime::Context engine;

        Host()
        : Host(std::make_unique<Provider>()) {}
        explicit Host(std::unique_ptr<Provider> resources)
        : provider(resources.get()),
          engine(
              std::make_unique<Display>(),
              std::make_unique<Surface>(),
              std::make_unique<Renderer>(),
              std::move(resources),
              std::make_unique<Input>()
          ) {}
    };

    struct Game final : CE::Game::AbstractGame {
        std::function<void()> initialize;
        std::function<void(const CE::Game::TickContext&)> advance;
        std::function<void()> cleanup;
        void init() override {
            if (initialize)
                initialize();
        }
        void update(const CE::Game::TickContext& tick) override {
            if (advance)
                advance(tick);
        }
        void deinit() override {
            if (cleanup)
                cleanup();
        }
        void prepare_render_frame(CE::Rendering::FrameWriter&) const override {}
    };

    std::string content(const std::shared_ptr<const Generation>& current) {
        std::string text;
        if (current) {
            for (const auto& scalar : current->layout().scalars())
                text += static_cast<char>(scalar.value);
        }
        return text;
    }

    bool settle(Updater& updater) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (updater.pending() && std::chrono::steady_clock::now() < deadline) {
            static_cast<void>(updater.update());
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        return !updater.pending();
    }
}

TEST(text_updater, initial) {
    Host host;
    Updater updater(host.engine, {.text = "Initial"});
    ASSERT_TRUE(updater.current());
    EXPECT_EQ(content(updater.current()), "Initial");
    EXPECT_FALSE(updater.pending());
    const auto current = updater.current();
    updater.request({.font = host.engine.fonts().default_font(), .text = "Initial"});
    EXPECT_FALSE(updater.pending());
    EXPECT_FALSE(updater.update());
    updater.retry();
    EXPECT_FALSE(updater.pending());
    updater.request({.text = "Different"});
    EXPECT_TRUE(updater.pending());
    updater.request({.text = "Initial"});
    EXPECT_FALSE(updater.pending());
    EXPECT_EQ(updater.current(), current);
    EXPECT_EQ(host.provider->uploads, 1);
    EXPECT_THROW(static_cast<void>(Updater(host.engine, {.text = "Bad", .layout = {.pixel_height = 0}})), CE::Exceptions::invalid_args);
}

TEST(text_updater, request_fields) {
    Host host;
    CE::Text::FontService other_fonts;
    const auto other_font = other_fonts.default_font();
    EXPECT_NE(other_font, host.engine.fonts().default_font());
    EXPECT_EQ(other_font.reference(), host.engine.fonts().default_font().reference());
    const std::vector<std::function<void(Request&)>> changes{
        [other_font](Request& request) { request.font = other_font; },
        [](Request& request) { request.text = "Different"; },
        [](Request& request) { request.layout.pixel_height = 24; },
        [](Request& request) { request.layout.direction = CE::Text::ParagraphDirection::RightToLeft; },
        [](Request& request) { request.layout.maximum_width = 160; },
        [](Request& request) { request.layout.language = "fr"; },
        [](Request& request) { request.layout.font_style.weight = 700; },
        [](Request& request) { request.layout.font_style.italic = true; },
        [](Request& request) { request.raster.maximum_page_extent = 2048; },
        [](Request& request) { request.raster.positioned_runs = true; },
        [&host](Request& request) { request.material = host.provider->builder.values.at(CE::Rendering::Materials::Builtin2D::Textured); }
    };
    for (std::size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        const Request initial{.text = "Same"};
        Updater updater(host.engine, initial);
        updater.request(initial);
        EXPECT_FALSE(updater.pending());
        auto changed = initial;
        changes[i](changed);
        updater.request(std::move(changed));
        EXPECT_TRUE(updater.pending());
    }
}

TEST(text_updater, coalescing) {
    for (const auto mode : {Mode::Sequential, Mode::Concurrent}) {
        Host host;
        Game game;
        std::unique_ptr<Updater> updater;
        std::vector<std::string> adopted;
        std::vector<CE::Rendering::DrawPacket2D> old_packets;
        std::weak_ptr<CE::Rendering::Geometry::Buffer2D> old_geometry;
        bool complete = false;
        game.initialize = [&] {
            updater = std::make_unique<Updater>(host.engine, Request{.text = "Zero"});
            old_geometry = updater->current()->pages()[0].geometry;
            old_packets = CE::Rendering::Submission::resolve_text(*updater->current(), {}, {});
            updater->request({.text = "One"});
            EXPECT_FALSE(updater->update());
            updater->request({.text = "Two"});
            updater->request({.text = "Three"});
        };
        game.advance = [&](const auto& tick) {
            if (updater->update())
                adopted.push_back(content(updater->current()));
            if (content(updater->current()) == "Three" && !updater->pending()) {
                EXPECT_EQ(adopted, (std::vector<std::string>{"One", "Three"}));
                EXPECT_FALSE(old_geometry.expired());
                old_packets.clear();
                EXPECT_TRUE(old_geometry.expired());
                EXPECT_EQ(host.provider->uploads, 3);
                updater->request({.text = "Three"});
                EXPECT_FALSE(updater->update());
                EXPECT_FALSE(updater->pending());
                complete = true;
                tick.request_stop();
            }
        };
        game.cleanup = [&] {
            EXPECT_TRUE(complete);
            updater.reset();
        };
        CE::Runtime::GameRuntime runtime(host.engine, game, mode);
        runtime.run();
    }
}

TEST(text_updater, returned_request) {
    for (const auto mode : {Mode::Sequential, Mode::Concurrent}) {
        Host host;
        Game game;
        std::unique_ptr<Updater> updater;
        bool complete = false;
        game.initialize = [&] {
            updater = std::make_unique<Updater>(host.engine);
            updater->request({.text = "One"});
            static_cast<void>(updater->update());
            updater->request({.text = "Two"});
            updater->request({.text = "One"});
        };
        game.advance = [&](const auto& tick) {
            if (updater->update()) {
                EXPECT_EQ(content(updater->current()), "One");
                EXPECT_FALSE(updater->pending());
                EXPECT_EQ(host.provider->uploads, 1);
                complete = true;
                tick.request_stop();
            }
        };
        game.cleanup = [&] {
            EXPECT_TRUE(complete);
            updater.reset();
        };
        CE::Runtime::GameRuntime runtime(host.engine, game, mode);
        runtime.run();
    }
}

TEST(text_updater, preparation_failure) {
    Host host;
    Updater updater(host.engine, {.text = "Old"});
    const auto previous = updater.current();
    Request bad{.text = "Bad", .layout = {.maximum_width = std::numeric_limits<float>::quiet_NaN()}};
    updater.request(bad);
    ASSERT_TRUE(settle(updater));
    ASSERT_TRUE(updater.failure());
    EXPECT_EQ(updater.failure()->stage, FailureStage::Preparation);
    EXPECT_THROW(std::rethrow_exception(updater.failure()->error), CE::Exceptions::invalid_args);
    const auto revision = updater.failure()->revision;
    updater.request(bad);
    EXPECT_FALSE(updater.pending());
    EXPECT_FALSE(updater.update());
    EXPECT_EQ(updater.failure()->revision, revision);
    EXPECT_EQ(updater.current(), previous);
    updater.retry();
    EXPECT_TRUE(updater.pending());
    EXPECT_FALSE(updater.failure());
    ASSERT_TRUE(settle(updater));
    ASSERT_TRUE(updater.failure());
    EXPECT_GT(updater.failure()->revision, revision);
    EXPECT_EQ(updater.current(), previous);
    EXPECT_EQ(host.provider->attempts, 1);
}

TEST(text_updater, upload_retry) {
    for (const auto mode : {Mode::Sequential, Mode::Concurrent}) {
        Host host;
        Game game;
        std::unique_ptr<Updater> updater;
        std::shared_ptr<const Generation> previous;
        std::uint64_t failed_revision = 0;
        bool retried = false, complete = false;
        game.initialize = [&] {
            updater = std::make_unique<Updater>(host.engine, Request{.text = "Old"});
            previous = updater->current();
            host.provider->fail = true;
            updater->request({.text = "New"});
        };
        game.advance = [&](const auto& tick) {
            static_cast<void>(updater->update());
            if (!retried && updater->failure()) {
                EXPECT_EQ(updater->failure()->stage, FailureStage::Upload);
                failed_revision = updater->failure()->revision;
                EXPECT_EQ(updater->current(), previous);
                updater->request({.text = "New"});
                EXPECT_FALSE(updater->pending());
                EXPECT_EQ(updater->failure()->revision, failed_revision);
                host.provider->fail = false;
                updater->retry();
                EXPECT_FALSE(updater->failure());
                retried = true;
            } else if (retried && content(updater->current()) == "New") {
                EXPECT_FALSE(updater->pending());
                EXPECT_FALSE(updater->failure());
                EXPECT_EQ(host.provider->attempts, 3);
                complete = true;
                tick.request_stop();
            }
        };
        game.cleanup = [&] {
            EXPECT_TRUE(complete);
            updater.reset();
        };
        CE::Runtime::GameRuntime runtime(host.engine, game, mode);
        runtime.run();
    }
}

TEST(text_updater, newer_after_failure) {
    for (const auto mode : {Mode::Sequential, Mode::Concurrent}) {
        Host host;
        Game game;
        std::unique_ptr<Updater> updater;
        bool complete = false;
        game.initialize = [&] {
            updater = std::make_unique<Updater>(host.engine, Request{.text = "Old"});
            updater->request({.text = "Bad", .layout = {.pixel_height = 0}});
            static_cast<void>(updater->update());
            updater->request({.text = "Good"});
        };
        game.advance = [&](const auto& tick) {
            static_cast<void>(updater->update());
            if (content(updater->current()) == "Good") {
                ASSERT_TRUE(updater->failure());
                EXPECT_EQ(updater->failure()->stage, FailureStage::Preparation);
                EXPECT_FALSE(updater->pending());
                updater->retry();
                EXPECT_FALSE(updater->pending());
                updater->request({.text = "Next"});
                EXPECT_FALSE(updater->failure());
                complete = true;
                tick.request_stop();
            }
        };
        game.cleanup = [&] {
            EXPECT_TRUE(complete);
            updater.reset();
        };
        CE::Runtime::GameRuntime runtime(host.engine, game, mode);
        runtime.run();
    }
}

TEST(text_updater, closed_dispatcher) {
    Host host;
    Updater updater(host.engine, {.text = "Old"});
    const auto previous = updater.current();
    updater.request({.text = "New"});
    EXPECT_TRUE(settle(updater));
    ASSERT_TRUE(updater.failure());
    EXPECT_EQ(updater.failure()->stage, FailureStage::Upload);
    EXPECT_THROW(std::rethrow_exception(updater.failure()->error), CE::Exceptions::failed_operation);
    EXPECT_EQ(updater.current(), previous);
    EXPECT_EQ(host.provider->uploads, 1);
}

TEST(text_updater, closed_workers) {
    for (const auto mode : {Mode::Sequential, Mode::Concurrent}) {
        Host host;
        Game game;
        std::unique_ptr<Updater> updater;
        game.initialize = [&] { updater = std::make_unique<Updater>(host.engine, Request{.text = "Old"}); };
        game.advance = [](const auto& tick) { tick.request_stop(); };
        game.cleanup = [&] {
            const auto previous = updater->current();
            updater->request({.text = "New"});
            EXPECT_FALSE(updater->update());
            EXPECT_FALSE(updater->pending());
            ASSERT_TRUE(updater->failure());
            EXPECT_EQ(updater->failure()->stage, FailureStage::Preparation);
            EXPECT_THROW(std::rethrow_exception(updater->failure()->error), CE::Exceptions::failed_operation);
            EXPECT_EQ(updater->current(), previous);
            updater.reset();
        };
        CE::Runtime::GameRuntime runtime(host.engine, game, mode);
        runtime.run();
    }
}

TEST(text_updater, destroy_pending) {
    Host host;
    auto blocker = host.engine.make_worker_group();
    std::promise<void> release;
    auto gate = release.get_future().share();
    auto blocked = blocker.submit([gate] { return gate.wait_for(timeout); });
    {
        CE::Text::FontService fonts;
        fonts.configure(CE::Text::FontConfiguration{});
        auto updater = std::make_unique<Updater>(host.engine);
        updater->request({.font = fonts.default_font(), .text = "Owned pending bytes"});
        static_cast<void>(updater->update());
        EXPECT_TRUE(updater->pending());
        updater.reset();
        EXPECT_EQ(blocked.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
    }
    release.set_value();
    EXPECT_EQ(blocked.get(), std::future_status::ready);
}

TEST(text_updater, cancelled_upload) {
    Host host;
    Game game;
    std::unique_ptr<Updater> updater;
    bool queued = false;
    game.initialize = [&] {
        updater = std::make_unique<Updater>(host.engine, Request{.text = "Old"});
        updater->request({.text = "New"});
    };
    game.advance = [&](const auto& tick) {
        static_cast<void>(updater->update());
        if (host.engine.platform_dispatcher().has_pending()) {
            queued = true;
            tick.request_stop();
        }
    };
    game.cleanup = [&] {
        EXPECT_TRUE(queued);
        EXPECT_FALSE(updater->update());
        ASSERT_TRUE(updater->failure());
        EXPECT_EQ(updater->failure()->stage, FailureStage::Upload);
        EXPECT_THROW(std::rethrow_exception(updater->failure()->error), std::future_error);
        EXPECT_EQ(content(updater->current()), "Old");
        updater.reset();
    };
    CE::Runtime::GameRuntime runtime(host.engine, game, Mode::Sequential);
    runtime.run();
}
