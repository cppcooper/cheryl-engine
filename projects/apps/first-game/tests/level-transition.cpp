#include "first-game.h"
#include "level-transition.h"

#include <cheryl/assets/resources/resource-provider.h>
#include <cheryl/core/controls/button-codes.h>
#include <cheryl/core/controls/input-interface.h>
#include <cheryl/core/display/display-system-interface.h>
#include <cheryl/core/game-framework/game-runtime.h>
#include <cheryl/core/rendering/presentation-surface.h>
#include <cheryl/core/rendering/renderer.h>
#include <testing/material-builder.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error First Game room tests must not depend on native graphics headers.
#endif

namespace {
    using namespace FirstGame::Levels;
    using Key = CE::Input::KeyboardKey;
    using Mode = CE::Runtime::RunMode;
    constexpr auto watchdog = std::chrono::seconds{30}; // Hang guard, never a race assertion.
    const auto assets = std::filesystem::path(CHERYL_SOURCE_DIR) / "projects/apps/first-game/assets";

    struct Geometry final : CE::Rendering::Geometry::Buffer2D {
        std::vector<CE::Rendering::Geometry::Vertex2D> vertices;
        CE::Rendering::Geometry::Topology primitive;
        Geometry(std::span<const CE::Rendering::Geometry::Vertex2D> values, CE::Rendering::Geometry::Topology topology)
        : vertices(values.begin(), values.end()), primitive(topology) {}
        CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override { return CE::Rendering::Geometry::Layout2D::Position3UV2; }
        CE::Rendering::Geometry::Topology topology() const noexcept override { return primitive; }
        std::size_t vertex_count() const noexcept override { return vertices.size(); }
        void bind() const override { throw std::logic_error("Headless test must not bind geometry"); }
        void draw(std::size_t, std::size_t) const override { throw std::logic_error("Headless test must not draw geometry"); }
    };
    struct Image final : CE::Rendering::Resources::Image {
        CE::Rendering::Geometry::PixelSize size;
        std::vector<unsigned char> pixels;
        Image(CE::Rendering::Geometry::PixelSize size, std::span<const unsigned char> values)
        : size(size), pixels(values.begin(), values.end()) {}
        CE::Rendering::Geometry::PixelSize pixel_size() const override { return size; }
        void bind(std::uint32_t) const override { throw std::logic_error("Headless test must not bind images"); }
    };
    struct Sampler final : CE::Rendering::Sampling::Sampler {
        explicit Sampler(CE::Rendering::Sampling::Options options)
        : CE::Rendering::Sampling::Sampler(options, 1) {}
        void bind(std::uint32_t) const override { throw std::logic_error("Headless test must not bind samplers"); }
    };
    struct Provider final : CE::Rendering::Resources::Provider {
        CE::Testing::MaterialBuilder builder = CE::Testing::ordinary_materials();
        const std::thread::id owner = std::this_thread::get_id();
        std::atomic<int> calls{0};
        std::atomic<int> fail_at{0};
        const std::exception_ptr injected = std::make_exception_ptr(std::runtime_error("private destination upload"));
        std::vector<std::weak_ptr<CE::Rendering::Geometry::Buffer2D>> geometries;
        std::vector<std::weak_ptr<CE::Rendering::Resources::Image>> images;
        std::weak_ptr<CE::Rendering::Geometry::Buffer2D> failed_transient;
        std::function<void()> atlas_entry; // Installed/used on platform only.

        void admit() {
            EXPECT_EQ(std::this_thread::get_id(), owner);
            if (++calls == fail_at.load()) {
                if (!geometries.empty())
                    failed_transient = geometries.back();
                std::rethrow_exception(injected);
            }
        }
        CE::Assets::Shaders::Builder* shader_asset_builder() noexcept override { return &builder; }
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded& decoded) override {
            admit();
            EXPECT_EQ(decoded.rgba.size(), static_cast<std::size_t>(decoded.size.width) * decoded.size.height * 4);
            auto result = std::make_shared<Image>(decoded.size, decoded.rgba);
            images.push_back(result);
            return result;
        }
        std::shared_ptr<const CE::Rendering::Sampling::Sampler> create_sampler(const CE::Rendering::Sampling::Options& options) override {
            EXPECT_EQ(std::this_thread::get_id(), owner);
            CE::Rendering::Sampling::validate(options);
            return std::make_shared<Sampler>(options);
        }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char> alpha, CE::Rendering::Geometry::PixelSize size) override {
            admit();
            if (atlas_entry)
                atlas_entry();
            EXPECT_EQ(alpha.size(), static_cast<std::size_t>(size.width) * size.height);
            auto result = std::make_shared<Image>(size, alpha);
            images.push_back(result);
            return result;
        }
        using CE::Rendering::Resources::Provider::upload_geometry;
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D> vertices, CE::Rendering::Geometry::Topology topology) override {
            admit();
            auto result = std::make_shared<Geometry>(vertices, topology);
            geometries.push_back(result);
            return result;
        }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override {
            throw std::logic_error("Bundled First Game uses built-in materials, not application programs");
        }
    };
    struct Window final : CE::Platform::Window {
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + watchdog;
        std::atomic<bool> expired{false};
        CE::Platform::Dimensions<int> logical_size() const override { return {640, 360}; }
        CE::Platform::FramebufferSize framebuffer_size() const override { return {640, 360}; }
        CE::Platform::WindowMode mode() const override { return CE::Platform::WindowMode::NORMAL; }
        bool should_close() const override {
            if (std::chrono::steady_clock::now() < deadline)
                return false;
            const_cast<Window*>(this)->expired = true;
            return true;
        }
        void resize(int, int) override {}
        void set_mode(CE::Platform::WindowMode) override {}
        void hide_cursor(bool) const override {}
    };
    struct Display final : CE::Platform::DisplaySystem {
        std::vector<CE::Platform::Monitor> inventory{{1, 640, 360}};
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
        struct Command { Key key; int mode; }; // 0=release, 1=hold, 2=tap.
        CE::Input::Actions::Bindings actions;
        std::mutex mutex;
        std::vector<Command> commands;
        bool attached = false;
        void send(Key key, int mode = 2) {
            std::lock_guard lock(mutex);
            commands.push_back({key, mode});
        }
        void initialize(CE::Platform::Window&) override { attached = true; }
        void deinitialize() override { attached = false; }
        void apply() {
            std::vector<Command> next;
            {
                std::lock_guard lock(mutex);
                next.swap(commands);
            }
            for (const auto command : next) {
                actions.on_button({keyboard_id(), command.key}, command.mode != 0);
                if (command.mode == 2)
                    actions.on_button({keyboard_id(), command.key}, false);
            }
        }
        void poll() override {
            apply();
            static_cast<void>(actions.publish_actions());
        }
        CE::Input::Actions::Bindings& bindings() override { return actions; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
    };
    struct Surface final : CE::Rendering::PresentationSurface {
        void present() override {}
    };
    struct Renderer final : CE::Rendering::Renderer {
        int renders = 0;
        int collections = 0;
        bool initialized = false;
        void initialize() override { initialized = true; }
        void deinitialize() override { initialized = false; }
        void maintain_resources() override { ++collections; }
        void render(const CE::Rendering::Frame& frame) override {
            EXPECT_EQ(frame.passes().size(), 1u);
            ++renders;
        }
        void clear() override {}
        void set_viewport(CE::Platform::FramebufferSize) override {}
        void set_depth_test(bool) override {}
        void set_clear_colour(float, float, float, float) override {}
        void set_camera_matrices(const glm::mat4&, const glm::mat4&) override {}
    };
    struct Host {
        Provider* provider;
        Input* input;
        Display* display;
        Renderer* renderer;
        CE::Runtime::Context engine;
        explicit Host(std::shared_ptr<CE::Workers::Pool> pool = {})
        : Host(std::make_unique<Provider>(), std::make_unique<Input>(), std::make_unique<Display>(), std::make_unique<Renderer>(), std::move(pool)) {}
        Host(std::unique_ptr<Provider> resources, std::unique_ptr<Input> controls, std::unique_ptr<Display> screen,
             std::unique_ptr<Renderer> graphics, std::shared_ptr<CE::Workers::Pool> pool)
        : provider(resources.get()), input(controls.get()), display(screen.get()), renderer(graphics.get()),
          engine(std::move(screen), std::make_unique<Surface>(), std::move(graphics), std::move(resources), std::move(controls), {.shared_pool = std::move(pool)}) {}
    };
    struct CpuGate {
        std::shared_ptr<CE::Workers::Pool> pool = std::make_shared<CE::Workers::Pool>(1);
        std::promise<void> entered;
        std::promise<void> release;
        std::shared_future<void> opened = release.get_future().share();
        CE::Workers::Group group = pool->make_group();
        std::future<void> work;
        std::atomic<bool> released{false};
        CpuGate() {
            auto arrival = entered.get_future();
            work = group.submit([this] {
                entered.set_value();
                if (opened.wait_for(watchdog) != std::future_status::ready)
                    throw std::runtime_error("CPU gate watchdog");
            });
            if (arrival.wait_for(watchdog) != std::future_status::ready)
                throw std::runtime_error("CPU gate did not start");
        }
        void open() {
            if (!released.exchange(true)) {
                release.set_value();
            }
        }
        ~CpuGate() { open(); group.close(); group.drain(); }
    };
    std::string content(const CE::Text::Rendering::Generation& generation) {
        std::string text;
        for (const auto& scalar : generation.layout().scalars())
            text += static_cast<char>(scalar.value);
        return text;
    }
    void frame(FirstGame::Game& game, CE::Rendering::Frame& output) {
        CE::Rendering::FrameWriter writer(output);
        game.prepare_render_frame(writer);
    }
    void manual_tick(FirstGame::Game& game, Input& input, double delta = 0.01) {
        const auto before = input.actions.action_snapshot();
        input.poll();
        CE::Input::Tick tick(before, {input.actions.action_snapshot()});
        game.update({delta, tick, {640, 360}});
    }
    void finish_manual(FirstGame::Game& game) {
        game.deinit_simulation();
        game.quiesce();
        game.deinit_platform();
    }

    enum class Scenario { Success, CancelQueued, UploadFailure, Unload, StopPending };

    void run_rooms(const Mode mode, const Scenario scenario) {
        CpuGate gate;
        Host host(gate.pool);
        FirstGame::Game game(host.engine, assets); // The production Game, never a surrogate.
        CE::Runtime::GameRuntime runtime(host.engine, game, mode);
        CE::Rendering::Frame held;
        std::weak_ptr<CE::Rendering::Geometry::Buffer2D> old_label;
        glm::mat4 old_model{1};
        bool saw_pending = false, saw_quiesce = false, finished = false, requested_reload = false;
        bool resuming = false;
        float resume_x = 0;
        int settled_calls = 0;
        int phase = 0;
        int initial_calls = 0;
        int pending_steps = 0;
        const auto ready = host.engine.events().register_listener(CE::Runtime::runtime_ready_event, [&](const auto&) {
            ASSERT_TRUE(game.current_level()); // RuntimeReady does not mark Study ready.
            EXPECT_EQ(game.current_level()->id, LevelId::Courtyard);
            initial_calls = host.provider->calls.load();
            if (scenario == Scenario::UploadFailure)
                host.provider->fail_at = host.provider->calls.load() + 2; // Geometry succeeds, then atlas fails.
            host.input->send(Key::Right, 1);
            host.input->send(Key::Space);
            host.input->send(Key::Tab);
        });
        const auto stopping = host.engine.events().register_listener(CE::Runtime::shutdown_starting_event, [&](const auto&) {
            gate.open(); // Never strand a gate if a watchdog or unexpected exception stops the session.
        });
        const auto quiescing = host.engine.events().register_listener(CE::Runtime::runtime_quiescing_event, [&](const auto&) {
            // Simulation has joined. Dependencies survive until final platform deinit.
            EXPECT_TRUE(game.transition_observation().closed);
            EXPECT_EQ(game.transition_observation().phase, Phase::Closed);
            EXPECT_TRUE(game.current_level());
            EXPECT_TRUE(game.actor());
            saw_quiesce = true;
        });
        const auto step = host.engine.events().register_listener(CE::Runtime::simulation_step_starting_event, [&](const auto&) {
            const auto observation = game.transition_observation();
            if (phase == 0 && observation.pending) {
                ASSERT_TRUE(game.current_level());
                EXPECT_EQ(game.current_level()->id, LevelId::Courtyard);
                EXPECT_EQ(content(game.current_level()->label), "Courtyard");
                ASSERT_TRUE(game.actor());
                if (++pending_steps < 2)
                    return; // Keep the CPU gate closed across another actual Game update.
                EXPECT_GT(game.actor()->position.x, 320);
                EXPECT_EQ(game.actor()->cell, 1u); // Space still works while the request is pending.
                old_label = game.current_level()->label.pages().front().geometry;
                frame(game, held);
                old_model = std::get<glm::mat4>(held.passes()[0].draws.front().parameters.at("model"));
                host.input->send(Key::Right, 0);
                saw_pending = true;
                phase = 1;
                if (scenario == Scenario::StopPending) {
                    finished = true;
                    runtime.stop(); // The runtime must invalidate/settle a live Game request.
                } else if (scenario == Scenario::CancelQueued)
                    host.input->send(Key::Backspace);
                else if (scenario == Scenario::Unload)
                    host.input->send(Key::U);
                else
                    gate.open();
            } else if (phase == 1 && scenario == Scenario::CancelQueued && observation.phase == Phase::Cancelled) {
                EXPECT_TRUE(observation.pending); // Cancel invalidates immediately, but accepted work is still busy.
                EXPECT_EQ(game.current_level()->id, LevelId::Courtyard);
                gate.open();
                phase = 2;
            } else if (phase == 1 && scenario == Scenario::Unload && !game.current_level()) {
                EXPECT_TRUE(observation.pending);
                EXPECT_FALSE(game.actor());
                CE::Rendering::Frame empty;
                frame(game, empty);
                ASSERT_EQ(empty.passes().size(), 1u);
                EXPECT_TRUE(empty.passes()[0].draws.empty());
                empty.recycle();
                gate.open();
                phase = 2;
            } else if (phase != 0 && !observation.pending && !finished) {
                if (scenario == Scenario::Success) {
                    ASSERT_TRUE(game.current_level());
                    EXPECT_EQ(game.current_level()->id, LevelId::Study);
                    EXPECT_EQ(content(game.current_level()->label), "Study");
                    ASSERT_TRUE(game.actor());
                    EXPECT_GE(game.actor()->position.x, 480); // The Right release may share the adoption input batch.
                    EXPECT_FLOAT_EQ(game.actor()->position.y, 180);
                    EXPECT_EQ(game.actor()->cell, 1u);
                    CE::Rendering::Frame destination;
                    frame(game, destination);
                    EXPECT_NE(destination.passes()[0].draws.back().geometry, held.passes()[0].draws.back().geometry);
                    destination.recycle();
                } else if (scenario == Scenario::UploadFailure) {
                    ASSERT_TRUE(observation.failure);
                    EXPECT_EQ(observation.phase, Phase::Failed);
                    EXPECT_EQ(observation.failure->stage, FailureStage::Upload);
                    EXPECT_EQ(observation.failure->id, observation.id);
                    EXPECT_EQ(observation.failure->error, host.provider->injected);
                    EXPECT_TRUE(host.provider->failed_transient.expired());
                    EXPECT_EQ(game.current_level()->id, LevelId::Courtyard);
                } else if (scenario == Scenario::CancelQueued) {
                    EXPECT_EQ(observation.phase, Phase::Cancelled);
                    EXPECT_EQ(game.current_level()->id, LevelId::Courtyard);
                    EXPECT_FALSE(observation.failure);
                    EXPECT_EQ(host.provider->calls.load(), initial_calls); // Pre-entry cancellation uploads nothing.
                } else if (!requested_reload) {
                    EXPECT_FALSE(game.current_level());
                    host.input->send(Key::Enter);
                    requested_reload = true;
                    return;
                } else {
                    if (!game.current_level())
                        return; // Await the input publication, not a wall-clock delay.
                    EXPECT_EQ(game.current_level()->id, LevelId::Courtyard);
                    EXPECT_FLOAT_EQ(game.actor()->position.y, 180);
                }
                if (scenario == Scenario::UploadFailure || scenario == Scenario::CancelQueued) {
                    ASSERT_TRUE(game.actor());
                    if (!resuming) {
                        resume_x = game.actor()->position.x;
                        settled_calls = host.provider->calls.load();
                        host.input->send(Key::Left, 1);
                        resuming = true;
                        return;
                    }
                    EXPECT_EQ(host.provider->calls.load(), settled_calls); // No automatic retry.
                    if (game.actor()->position.x >= resume_x)
                        return; // Await an actual update consuming the movement binding.
                    EXPECT_LT(game.actor()->position.x, resume_x);
                    host.input->send(Key::Left, 0);
                }
                EXPECT_FALSE(old_label.expired());
                EXPECT_EQ(std::get<glm::mat4>(held.passes()[0].draws.front().parameters.at("model")), old_model);
                finished = true;
                host.input->send(Key::Escape); // Real binding and TickContext stop path.
            }
        });
        try {
            runtime.run();
        } catch (...) {
            gate.open();
            throw;
        }
        gate.open();
        EXPECT_TRUE(saw_pending);
        EXPECT_TRUE(saw_quiesce);
        EXPECT_TRUE(finished);
        EXPECT_FALSE(host.display->window.expired);
        EXPECT_FALSE(host.input->attached);
        EXPECT_FALSE(host.renderer->initialized);
        EXPECT_GT(host.renderer->collections, 0);
        EXPECT_FALSE(game.current_level());
        EXPECT_FALSE(old_label.expired()); // The explicit A frame survives B and full game teardown.
        held.recycle();
        EXPECT_TRUE(old_label.expired());
        EXPECT_EQ(runtime.lifecycle().snapshot().outcome, CE::Runtime::RuntimeOutcome::Completed);
        static_cast<void>(host.engine.events().unregister_listener(step));
        static_cast<void>(host.engine.events().unregister_listener(quiescing));
        static_cast<void>(host.engine.events().unregister_listener(stopping));
        static_cast<void>(host.engine.events().unregister_listener(ready));
    }
}

TEST(first_game_levels, room_specs) {
    const auto courtyard = room_spec(LevelId::Courtyard);
    const auto study = room_spec(LevelId::Study);
    EXPECT_EQ(courtyard.label, "Courtyard");
    EXPECT_EQ(study.label, "Study");
    EXPECT_FLOAT_EQ(courtyard.spawn.x, 0.5f);
    EXPECT_FLOAT_EQ(study.spawn.x, 0.75f);
    EXPECT_FLOAT_EQ(courtyard.spawn.y, 0.5f);
    EXPECT_FLOAT_EQ(study.spawn.y, 0.5f);
    EXPECT_EQ(courtyard.cell, 0u);
    EXPECT_EQ(study.cell, 1u);
    EXPECT_THROW(static_cast<void>(room_spec(static_cast<LevelId>(42))), std::invalid_argument);
}

TEST(first_game_levels, actual_game_empty_and_controls) {
    Host host;
    FirstGame::Game game(host.engine, assets);
    game.init_platform();
    ASSERT_TRUE(game.current_level());
    EXPECT_EQ(content(game.current_level()->label), "Courtyard");
    ASSERT_TRUE(game.actor());
    EXPECT_FLOAT_EQ(game.actor()->position.x, 320);
    EXPECT_FLOAT_EQ(game.actor()->position.y, 180);
    host.input->send(Key::Right, 1);
    host.input->send(Key::Space);
    manual_tick(game, *host.input);
    EXPECT_GT(game.actor()->position.x, 320);
    EXPECT_EQ(game.actor()->cell, 1u);
    host.input->send(Key::Right, 0);
    host.input->send(Key::U);
    manual_tick(game, *host.input);
    EXPECT_FALSE(game.current_level());
    EXPECT_FALSE(game.actor());
    EXPECT_EQ(game.transition_observation().phase, Phase::Idle);
    CE::Rendering::Frame empty;
    frame(game, empty);
    ASSERT_EQ(empty.passes().size(), 1u);
    EXPECT_TRUE(empty.passes()[0].draws.empty());
    empty.recycle();
    finish_manual(game);
    finish_manual(game); // Partial/idempotent cleanup, including all eight bindings.
    host.input->send(Key::Tab);
    host.input->send(Key::Space);
    host.input->poll();
    EXPECT_FALSE(host.input->actions.action_snapshot()->button(CE::Input::Actions::Id{5}).pressed());
    EXPECT_FALSE(host.input->actions.action_snapshot()->button(CE::Input::Actions::Id{4}).pressed());
}

TEST(first_game_levels, partial_initialization_cleanup) {
    Host host;
    host.provider->fail_at = 1;
    FirstGame::Game game(host.engine, assets);
    EXPECT_THROW(game.init_platform(), std::runtime_error);
    game.quiesce();
    game.deinit_platform();
    host.input->send(Key::Tab);
    host.input->poll();
    EXPECT_FALSE(host.input->actions.action_snapshot()->button(CE::Input::Actions::Id{5}).pressed());
    EXPECT_FALSE(game.current_level());
}

TEST(first_game_levels, runtime_success_sequential) { run_rooms(Mode::Sequential, Scenario::Success); }
TEST(first_game_levels, runtime_success_concurrent) { run_rooms(Mode::Concurrent, Scenario::Success); }
TEST(first_game_levels, runtime_cancel_sequential) { run_rooms(Mode::Sequential, Scenario::CancelQueued); }
TEST(first_game_levels, runtime_cancel_concurrent) { run_rooms(Mode::Concurrent, Scenario::CancelQueued); }
TEST(first_game_levels, runtime_failure_sequential) { run_rooms(Mode::Sequential, Scenario::UploadFailure); }
TEST(first_game_levels, runtime_failure_concurrent) { run_rooms(Mode::Concurrent, Scenario::UploadFailure); }
TEST(first_game_levels, runtime_unload_sequential) { run_rooms(Mode::Sequential, Scenario::Unload); }
TEST(first_game_levels, runtime_unload_concurrent) { run_rooms(Mode::Concurrent, Scenario::Unload); }
TEST(first_game_levels, runtime_pending_exit_sequential) { run_rooms(Mode::Sequential, Scenario::StopPending); }
TEST(first_game_levels, runtime_pending_exit_concurrent) { run_rooms(Mode::Concurrent, Scenario::StopPending); }

TEST(first_game_levels, controller_cancel_busy_and_close) {
    CpuGate gate;
    Host host(gate.pool);
    FirstGame::Game game(host.engine, assets);
    game.init_platform();
    const auto original = game.current_level();
    Controller controller(host.engine, original->sprite, original->images);
    EXPECT_EQ(controller.request(LevelId::Courtyard).disposition, Disposition::AlreadyCurrent);
    EXPECT_THROW(static_cast<void>(controller.request(static_cast<LevelId>(9))), std::invalid_argument);
    EXPECT_EQ(controller.observation().id, 0u);
    const int uploads = host.provider->calls.load();
    const auto first = controller.request(LevelId::Study);
    EXPECT_EQ(first.disposition, Disposition::Accepted);
    EXPECT_NE(first.id, 0u);
    EXPECT_EQ(controller.cancel(first.id + 1), CancelResult::NotPending);
    EXPECT_EQ(controller.cancel(first.id), CancelResult::Cancelled);
    EXPECT_EQ(controller.cancel(first.id), CancelResult::NotPending);
    EXPECT_EQ(controller.request(LevelId::Study).disposition, Disposition::Busy);
    EXPECT_FALSE(controller.advance());
    EXPECT_TRUE(controller.observation().pending);
    EXPECT_EQ(controller.current()->id, LevelId::Courtyard);
    gate.open();
    const auto deadline = std::chrono::steady_clock::now() + watchdog;
    while (controller.observation().pending && std::chrono::steady_clock::now() < deadline) {
        EXPECT_FALSE(controller.advance());
        std::this_thread::yield();
    }
    EXPECT_FALSE(controller.observation().pending);
    EXPECT_EQ(host.provider->calls.load(), uploads);
    const auto retry = controller.request(LevelId::Study);
    EXPECT_EQ(retry.disposition, Disposition::Accepted);
    EXPECT_GT(retry.id, first.id);
    controller.unload();
    EXPECT_FALSE(controller.current());
    controller.close();
    controller.close();
    EXPECT_EQ(controller.request(LevelId::Study).disposition, Disposition::Closed);
    EXPECT_EQ(controller.observation().phase, Phase::Closed);
    finish_manual(game);
}

TEST(first_game_levels, rejected_platform_preserves_current) {
    Host host; // No runtime session: the real dispatcher is closed, not a fake submitter.
    FirstGame::Game game(host.engine, assets);
    game.init_platform();
    const auto initial = game.current_level();
    Controller controller(host.engine, initial->sprite, initial->images);
    const auto request = controller.request(LevelId::Study);
    const auto deadline = std::chrono::steady_clock::now() + watchdog;
    while (controller.observation().pending && std::chrono::steady_clock::now() < deadline) {
        EXPECT_FALSE(controller.advance());
        std::this_thread::yield();
    }
    const auto result = controller.observation();
    EXPECT_FALSE(result.pending);
    EXPECT_EQ(result.phase, Phase::Failed);
    ASSERT_TRUE(result.failure);
    EXPECT_EQ(result.failure->id, request.id);
    EXPECT_EQ(result.failure->stage, FailureStage::PlatformSubmission);
    EXPECT_TRUE(result.failure->error);
    EXPECT_EQ(controller.current()->id, LevelId::Courtyard);
    const auto retained = result.failure->error;
    EXPECT_FALSE(controller.advance());
    EXPECT_EQ(controller.observation().failure->error, retained); // No auto-retry.
    controller.close();
    finish_manual(game);
}

TEST(first_game_levels, rejected_cpu_preserves_current) {
    auto pool = std::make_shared<CE::Workers::Pool>(1);
    Host host(pool);
    FirstGame::Game game(host.engine, assets);
    game.init_platform();
    const auto initial = game.current_level();
    Controller controller(host.engine, initial->sprite, initial->images);
    pool->close(); // Real Group rejection; no production-controller testing hook.
    const auto request = controller.request(LevelId::Study);
    EXPECT_EQ(request.disposition, Disposition::Accepted);
    const auto result = controller.observation();
    EXPECT_FALSE(result.pending);
    EXPECT_EQ(result.phase, Phase::Failed);
    ASSERT_TRUE(result.failure);
    EXPECT_EQ(result.failure->id, request.id);
    EXPECT_EQ(result.failure->stage, FailureStage::PreparationSubmission);
    EXPECT_TRUE(result.failure->error);
    EXPECT_EQ(controller.current()->id, LevelId::Courtyard);
    EXPECT_FALSE(controller.advance());
    EXPECT_EQ(controller.observation().failure->error, result.failure->error);
    controller.close();
    finish_manual(game);
}

TEST(first_game_levels, cancelled_during_upload_is_not_rollback) {
    Host host;
    FirstGame::Game game(host.engine, assets);
    CE::Runtime::GameRuntime runtime(host.engine, game, Mode::Concurrent);
    std::unique_ptr<Controller> controller;
    bool cancelled = false, settled = false;
    int initial_calls = 0;
    const auto ready = host.engine.events().register_listener(CE::Runtime::runtime_ready_event, [&](const auto&) {
        const auto initial = game.current_level();
        controller = std::make_unique<Controller>(host.engine, initial->sprite, initial->images);
        initial_calls = host.provider->calls.load();
        const auto request = controller->request(LevelId::Study);
        host.provider->atlas_entry = [&, request] {
            // Called after destination geometry exists, on platform. Invalidation
            // is dispatched to the real simulation owner, never concurrent access.
            auto result = runtime.simulation_dispatcher().submit([&, request] {
                EXPECT_EQ(controller->cancel(request.id), CancelResult::Cancelled);
                EXPECT_EQ(controller->request(LevelId::Study).disposition, Disposition::Busy);
                cancelled = true;
            });
            if (result.wait_for(watchdog) != std::future_status::ready)
                throw std::runtime_error("Upload cancellation watchdog");
            result.get();
        };
    });
    const auto step = host.engine.events().register_listener(CE::Runtime::simulation_step_starting_event, [&](const auto&) {
        EXPECT_FALSE(controller->advance());
        if (cancelled && !controller->observation().pending) {
            EXPECT_EQ(controller->current()->id, LevelId::Courtyard);
            EXPECT_EQ(controller->observation().phase, Phase::Cancelled);
            settled = true;
            controller->close();
            runtime.stop();
        }
    });
    runtime.run();
    EXPECT_TRUE(cancelled);
    EXPECT_TRUE(settled);
    EXPECT_GE(host.provider->calls.load(), initial_calls + 2); // Executing upload finished; it was not undone.
    EXPECT_FALSE(host.display->window.expired);
    static_cast<void>(host.engine.events().unregister_listener(step));
    static_cast<void>(host.engine.events().unregister_listener(ready));
    host.provider->atlas_entry = {};
    controller.reset();
}

TEST(first_game_levels, completed_unadopted_is_cancellable) {
    Host host;
    FirstGame::Game game(host.engine, assets);
    CE::Runtime::GameRuntime runtime(host.engine, game, Mode::Concurrent);
    std::unique_ptr<Controller> controller;
    RequestId request = 0;
    std::uint64_t completed_before = 0;
    bool cancelled = false, settled = false;
    const auto ready = host.engine.events().register_listener(CE::Runtime::runtime_ready_event, [&](const auto&) {
        const auto initial = game.current_level();
        controller = std::make_unique<Controller>(host.engine, initial->sprite, initial->images);
        completed_before = host.engine.platform_dispatcher().diagnostics().completed;
        request = controller->request(LevelId::Study).id;
    });
    const auto step = host.engine.events().register_listener(CE::Runtime::simulation_step_starting_event, [&](const auto&) {
        if (controller->observation().phase == Phase::Uploading && !cancelled) {
            // Do not poll/adopt until the real platform callback has completed.
            // No sleep or assumed upload speed establishes this boundary.
            if (host.engine.platform_dispatcher().diagnostics().completed == completed_before)
                return;
            EXPECT_EQ(controller->current()->id, LevelId::Courtyard);
            EXPECT_EQ(controller->cancel(request), CancelResult::Cancelled);
            cancelled = true;
        }
        EXPECT_FALSE(controller->advance());
        if (cancelled && !controller->observation().pending) {
            EXPECT_EQ(controller->current()->id, LevelId::Courtyard);
            EXPECT_EQ(controller->observation().phase, Phase::Cancelled);
            EXPECT_EQ(controller->cancel(request), CancelResult::NotPending);
            settled = true;
            controller->close();
            runtime.stop();
        }
    });
    runtime.run();
    EXPECT_TRUE(cancelled);
    EXPECT_TRUE(settled);
    EXPECT_FALSE(host.display->window.expired);
    static_cast<void>(host.engine.events().unregister_listener(step));
    static_cast<void>(host.engine.events().unregister_listener(ready));
    controller.reset();
}

TEST(first_game_levels, actual_game_exit_during_upload) {
    Host host;
    FirstGame::Game game(host.engine, assets);
    CE::Runtime::GameRuntime runtime(host.engine, game, Mode::Concurrent);
    bool stopped_during_upload = false;
    std::weak_ptr<CE::Rendering::Geometry::Buffer2D> destination_geometry;
    const auto ready = host.engine.events().register_listener(CE::Runtime::runtime_ready_event, [&](const auto&) {
        host.provider->atlas_entry = [&] {
            destination_geometry = host.provider->geometries.back();
            auto stop = runtime.simulation_dispatcher().submit([&] {
                EXPECT_TRUE(game.transition_observation().pending);
                EXPECT_EQ(game.current_level()->id, LevelId::Courtyard);
                stopped_during_upload = true;
                runtime.stop(); // Real Game deinit must not wait for this platform callback.
            });
            if (stop.wait_for(watchdog) != std::future_status::ready)
                throw std::runtime_error("Upload exit watchdog");
            stop.get();
        };
        host.input->send(Key::Tab);
    });
    runtime.run();
    EXPECT_TRUE(stopped_during_upload);
    EXPECT_FALSE(host.display->window.expired);
    EXPECT_FALSE(game.current_level());
    EXPECT_TRUE(destination_geometry.expired());
    EXPECT_FALSE(host.input->attached);
    EXPECT_FALSE(host.renderer->initialized);
    EXPECT_EQ(runtime.lifecycle().snapshot().outcome, CE::Runtime::RuntimeOutcome::Completed);
    static_cast<void>(host.engine.events().unregister_listener(ready));
    host.provider->atlas_entry = {};
}
