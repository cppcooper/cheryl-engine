#include <core/engine/engine-context.h>
#include <core/game-framework/abstract-game.h>
#include <core/game-framework/game-runtime.h>
#include <core/game-framework/game-runtime-internal.h>
#include <core/controls/input-interface.h>
#include <core/display/display-system-interface.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <assets/resources/resource-provider.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#if defined(GL_VERSION_3_3) || defined(GLFW_VERSION_MAJOR)
#error Engine runtime contracts must compile without native or graphics SDK headers.
#endif

namespace {
    struct Observations {
        CE::Platform::Window* attached = nullptr;
        CE::Platform::FramebufferSize viewport{};
        int updates = 0;
        int frames = 0;
        int renders = 0;
        int presentations = 0;
        int initializations = 0;
        int shutdowns = 0;
        int game_initializations = 0;
        int quiesces = 0;
        int game_shutdowns = 0;
        bool pressed = false;
        bool sample_gamepad = false;
        bool observe_graphics = false;
        std::stop_source saved_stop{std::nostopstate};
    };

    class ContractWindow final : public CE::Platform::Window {
        const std::thread::id owner_ = std::this_thread::get_id();
        const std::chrono::steady_clock::time_point deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds{5};

    public:
        CE::Platform::Dimensions<int> logical_size() const override {
            EXPECT_EQ(std::this_thread::get_id(), owner_);
            return {16, 12};
        }
        CE::Platform::FramebufferSize framebuffer_size() const override {
            EXPECT_EQ(std::this_thread::get_id(), owner_);
            return {32, 24};
        }
        CE::Platform::WindowMode mode() const override { return CE::Platform::WindowMode::NORMAL; }
        bool should_close() const override { return std::chrono::steady_clock::now() >= deadline_; }
        void resize(int, int) override {}
        void set_mode(CE::Platform::WindowMode) override {}
        void hide_cursor(bool) const override {}
    };

    class ContractDisplay final : public CE::Platform::DisplaySystem {
        std::vector<CE::Platform::Monitor> monitors_{{1, 32, 24}};
        ContractWindow window_;

    public:
        const std::vector<CE::Platform::Monitor>& monitors() const override { return monitors_; }
        int monitor_count() const override { return 1; }
        const CE::Platform::Monitor& primary_monitor() const override { return monitors_.front(); }
        CE::Platform::Window* active_window() const override { return const_cast<ContractWindow*>(&window_); }
        std::pair<float, float> content_scale(const CE::Platform::Monitor&) const override { return {1, 1}; }
        CE::Platform::Window* create_window(const CE::Platform::Monitor&, CE::Platform::WindowMode, int, int) override { return &window_; }
        void activate_window(CE::Platform::Window&) override {}
    };

    class ContractInput final : public CE::Input::System {
        Observations& observations_;
        CE::Input::Actions::Bindings bindings_;

    public:
        explicit ContractInput(Observations& observations)
        : observations_(observations) {}
        void initialize(CE::Platform::Window& window) override {
            if (observations_.sample_gamepad)
                open_gamepad_availability();
            observations_.attached = &window;
        }
        void deinitialize() override {
            observations_.attached = nullptr;
            if (observations_.sample_gamepad)
                discard_captured_input();
        }
        void poll() override {
            bindings_.on_button({keyboard_id(), 65}, true);
            if (observations_.sample_gamepad) {
                stage_gamepad_availability(gamepad_id(), true);
                static_cast<void>(publish_input());
                return;
            }
            (void)bindings_.publish_actions();
        }
        CE::Input::Actions::Bindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
    };

    class ContractSurface final : public CE::Rendering::PresentationSurface {
        Observations& observations_;

    public:
        explicit ContractSurface(Observations& observations)
        : observations_(observations) {}
        void present() override { ++observations_.presentations; }
    };

    class ContractRenderer final : public CE::Rendering::Renderer {
        Observations& observations_;
        std::unique_ptr<CE::Rendering::GraphicsDomainOwner> domain_;

    public:
        explicit ContractRenderer(Observations& observations)
        : observations_(observations) {
            if (observations.observe_graphics)
                domain_ = std::make_unique<CE::Rendering::GraphicsDomainOwner>();
        }
        CE::Rendering::GraphicsDomain graphics_domain() const noexcept override {
            return domain_ ? domain_->token() : CE::Rendering::GraphicsDomain{};
        }
        void initialize() override {
            ++observations_.initializations;
            if (domain_)
                domain_->make_available();
            publish_graphics_domain_changes();
        }
        void deinitialize() override {
            ++observations_.shutdowns;
            if (domain_)
                domain_->shutdown();
            publish_graphics_domain_changes();
        }
        void maintain_resources() override {}
        void render(const CE::Rendering::Frame& frame) override {
            EXPECT_EQ(frame.passes().size(), 1u);
            ++observations_.renders;
        }
        void clear() override {}
        void set_viewport(CE::Platform::FramebufferSize size) override { observations_.viewport = size; }
        void set_depth_test(bool) override {}
        void set_clear_colour(float, float, float, float) override {}
        void set_camera_matrices(const glm::mat4&, const glm::mat4&) override {}
    };

    class ContractResources final : public CE::Rendering::Resources::Provider {
    public:
        std::shared_ptr<CE::Rendering::Resources::Image> create_image(const CE::Assets::Images::Decoded&) override {
            throw std::logic_error("This contract scenario does not upload images");
        }
        std::shared_ptr<CE::Rendering::Resources::Image>
        create_font_atlas(std::span<const unsigned char>, CE::Rendering::Geometry::PixelSize) override {
            throw std::logic_error("This contract scenario does not upload fonts");
        }
        std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
        upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D>, CE::Rendering::Geometry::Topology) override {
            throw std::logic_error("This contract scenario does not upload geometry");
        }
        std::shared_ptr<CE::Rendering::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override {
            throw std::logic_error("This contract scenario does not link shaders");
        }
    };

    class ContractGame final : public CE::Game::AbstractGame {
        CE::Runtime::Context& engine_;
        Observations& observations_;

    public:
        ContractGame(CE::Runtime::Context& engine, Observations& observations)
        : engine_(engine), observations_(observations) {}
        void init() override {
            ++observations_.game_initializations;
            (void)engine_.input().bindings().bind_button({1, 65}, CE::Input::Actions::Id{1});
        }
        void quiesce() override {
            EXPECT_TRUE(observations_.saved_stop.stop_requested());
            EXPECT_NE(observations_.attached, nullptr);
            EXPECT_EQ(observations_.shutdowns, 0);
            ++observations_.quiesces;
        }
        void deinit() override {
            EXPECT_EQ(observations_.quiesces, 1);
            EXPECT_NE(observations_.attached, nullptr);
            EXPECT_EQ(observations_.shutdowns, 0);
            ++observations_.game_shutdowns;
        }
        void update(const CE::Game::TickContext& tick) override {
            observations_.pressed = tick.input.button(CE::Input::Actions::Id{1}).pressed();
            EXPECT_EQ(tick.framebuffer_size, (CE::Platform::FramebufferSize{32, 24}));
            EXPECT_EQ(tick.logical_size.width, 16);
            EXPECT_EQ(tick.logical_size.height, 12);
            ++observations_.updates;
            observations_.saved_stop = tick.runtime_stop;
            tick.request_stop();
            tick.request_stop(); // Repeated requests preserve ordinary cleanup.
        }
        void prepare_render_frame(CE::Rendering::FrameWriter& frame) const override {
            (void)frame.begin_pass(glm::mat4{1}, glm::mat4{1});
            ++observations_.frames;
        }
    };
}

TEST(runtime_contract, frame) {
    Observations observations;
    CE::Runtime::Context engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    ContractGame game(engine, observations);
    EXPECT_EQ(engine.fonts().default_font().reference(), "cheryl:embedded");
    CE::Runtime::GameRuntime runtime(engine, game);
    runtime.run();
    EXPECT_TRUE(observations.pressed);
    EXPECT_EQ(observations.updates, 1);
    EXPECT_EQ(observations.frames, 1);
    EXPECT_EQ(observations.renders, 1);
    EXPECT_EQ(observations.presentations, 1);
    EXPECT_EQ(observations.initializations, 1);
    EXPECT_EQ(observations.shutdowns, 1);
    EXPECT_EQ(observations.game_initializations, 1);
    EXPECT_EQ(observations.quiesces, 1);
    EXPECT_EQ(observations.game_shutdowns, 1);
    EXPECT_EQ(observations.viewport, (CE::Platform::FramebufferSize{32, 24}));
    EXPECT_EQ(observations.attached, nullptr);
    const auto diagnostics = runtime.diagnostics();
    EXPECT_EQ(diagnostics.updates, 1u);
    EXPECT_EQ(diagnostics.published, 1u);
    EXPECT_EQ(diagnostics.rendered, 1u);
    EXPECT_THROW(runtime.run(), CE::Exceptions::failed_operation);
}

TEST(runtime_contract, gamepad_notifications) {
    for (const auto mode : std::array{CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        Observations observations;
        observations.sample_gamepad = true;
        CE::Runtime::Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        std::vector<CE::Input::GamepadAvailability> changes;
        const auto owner = std::this_thread::get_id();
        const auto id = engine.events().register_listener(
            CE::Input::gamepad_availability_changed_event,
            [&](const CE::Input::GamepadAvailabilityChanged& change) {
                EXPECT_EQ(std::this_thread::get_id(), owner);
                if (!change.current.closed)
                    EXPECT_EQ(change.current.poll, engine.input().poll_snapshot()->state->poll());
                changes.push_back(change.current);
            }
        );
        ContractGame game(engine, observations);
        CE::Runtime::GameRuntime runtime(engine, game, mode);
        runtime.run();
        ASSERT_EQ(changes.size(), 2u);
        EXPECT_TRUE(changes.front().available);
        EXPECT_FALSE(changes.front().closed);
        EXPECT_TRUE(changes.back().available);
        EXPECT_TRUE(changes.back().closed);
        EXPECT_TRUE(engine.events().unregister_and_wait(id));
    }
}

TEST(runtime_contract, graphics_events) {
    for (const auto mode : {CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        Observations observations;
        observations.observe_graphics = true;
        CE::Runtime::Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        const auto domain = engine.renderer().graphics_domain();
        const auto owner = std::this_thread::get_id();
        std::vector<CE::Rendering::GraphicsDomainSnapshot> changes;
        engine.events().register_listener(
            CE::Rendering::graphics_domain_state_changed_event,
            [](const CE::Rendering::GraphicsDomainStateChanged&) { throw std::runtime_error("observer failure"); }
        );
        engine.events().register_listener(
            CE::Rendering::graphics_domain_state_changed_event, [&](const CE::Rendering::GraphicsDomainStateChanged& change) {
                EXPECT_EQ(std::this_thread::get_id(), owner);
                EXPECT_EQ(change.current, domain.snapshot());
                changes.push_back(change.current);
            }
        );
        ContractGame game(engine, observations);
        CE::Runtime::GameRuntime runtime(engine, game, mode);
        EXPECT_NO_THROW(runtime.run());
        ASSERT_EQ(changes.size(), 2u);
        EXPECT_EQ(changes[0].reason, CE::Rendering::GraphicsDomainReason::Initialized);
        EXPECT_EQ(changes[1].reason, CE::Rendering::GraphicsDomainReason::Shutdown);
        EXPECT_EQ(changes[0].id, changes[1].id);
    }
}

TEST(runtime_contract, concurrent_window_snapshot) {
    Observations observations;
    CE::Runtime::Context engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    ContractGame game(engine, observations);
    CE::Runtime::GameRuntime runtime(engine, game, CE::Runtime::RunMode::Concurrent);
    runtime.run();
    EXPECT_EQ(observations.updates, 1);
    EXPECT_EQ(observations.game_initializations, 1);
    EXPECT_EQ(observations.quiesces, 1);
    EXPECT_EQ(observations.game_shutdowns, 1);
    EXPECT_EQ(observations.shutdowns, 1);
    EXPECT_EQ(observations.attached, nullptr);
}

TEST(runtime_contract, stopped) {
    for (const auto mode : std::array{CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        Observations observations;
        CE::Runtime::Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        ContractGame game(engine, observations);
        CE::Runtime::GameRuntime runtime(engine, game, mode);
        runtime.stop();
        runtime.stop();
        runtime.run();
        EXPECT_EQ(observations.updates, 0);
        EXPECT_EQ(observations.initializations, 0);
        EXPECT_EQ(observations.game_initializations, 0);
        EXPECT_EQ(observations.quiesces, 0);
        EXPECT_EQ(observations.game_shutdowns, 0);
        EXPECT_EQ(observations.shutdowns, 0);
        EXPECT_EQ(observations.attached, nullptr);
    }
}

TEST(runtime_contract, saved_stop) {
    Observations observations;
    {
        CE::Runtime::Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        ContractGame game(engine, observations);
        CE::Runtime::GameRuntime runtime(engine, game);
        runtime.run();
    }
    EXPECT_TRUE(observations.saved_stop.stop_requested());
    EXPECT_FALSE(observations.saved_stop.request_stop());
    EXPECT_EQ(observations.game_shutdowns, 1);
    EXPECT_EQ(observations.shutdowns, 1);
}

TEST(runtime_contract, missing_display) {
    Observations observations;
    EXPECT_THROW(
        CE::Runtime::Context(
            nullptr, std::make_unique<ContractSurface>(observations), std::make_unique<ContractRenderer>(observations),
            std::make_unique<ContractResources>(), std::make_unique<ContractInput>(observations)
        ),
        CE::Exceptions::invalid_args
    );
}

namespace {
    using PressureAccess = CE::Runtime::Detail::GameRuntimeAccess;
    using PressureChange = CE::Runtime::Detail::RuntimePressureChanged;

    struct PressureFixture {
        Observations observations;
        CE::Runtime::Context engine{
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        };
        ContractGame game{engine, observations};
        CE::Runtime::GameRuntime runtime{engine, game};
        std::shared_ptr<CE::Runtime::Detail::RuntimePressureState> state = PressureAccess::pressure_state(runtime);
    };
}

TEST(runtime_pressure, runtime_owners) {
    using Change = CE::Runtime::Detail::RuntimePressureChanged;
    for (const auto mode : std::array{CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        Observations observations;
        CE::Runtime::Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        struct PressureGame final : CE::Game::AbstractGame {
            std::shared_ptr<CE::Runtime::Detail::RuntimePressureState> state;
            std::atomic<bool> observed{false};

            void init() override {}
            void deinit() override {}
            void update(const CE::Game::TickContext&) override { state->stage_lag(!observed.load(), 2, 100); }
            void prepare_render_frame(CE::Rendering::FrameWriter& frame) const override {
                static_cast<void>(frame.begin_pass(glm::mat4{1}, glm::mat4{1}));
            }
        } game;
        CE::Runtime::GameRuntime runtime(engine, game, mode);
        game.state = CE::Runtime::Detail::GameRuntimeAccess::pressure_state(runtime);
        const auto platform_owner = std::this_thread::get_id();
        int calls = 0;
        engine.events().register_listener(CE::Runtime::Detail::runtime_pressure_changed_event, [](const Change&) {
            throw std::runtime_error("optional runtime observer");
        });
        const auto watch = CE::Runtime::Detail::GameRuntimeAccess::watch_pressure(runtime, [&](const Change& change) {
            EXPECT_EQ(std::this_thread::get_id(), platform_owner);
            ++calls;
            if (change.current.conditions.simulation_lag)
                game.observed.store(true);
            else
                runtime.stop();
        });
        EXPECT_NO_THROW(runtime.run());
        EXPECT_EQ(calls, 2);
        EXPECT_TRUE(game.state->current().closed);
        EXPECT_FALSE(game.state->current().conditions.simulation_lag);
        EXPECT_FALSE(engine.events().diagnostics().closed);
        EXPECT_TRUE(engine.events().unregister_and_wait(watch.registration));
    }
}

TEST(runtime_pressure, unstarted) {
    Observations observations;
    CE::Runtime::Context engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    ContractGame game(engine, observations);
    CE::Runtime::GameRuntime runtime(engine, game);
    const auto state = CE::Runtime::Detail::GameRuntimeAccess::pressure_state(runtime);
    int calls = 0;
    const auto watch = CE::Runtime::Detail::GameRuntimeAccess::watch_pressure(
        runtime, [&](const CE::Runtime::Detail::RuntimePressureChanged&) { ++calls; }
    );
    runtime.stop();
    runtime.run();
    EXPECT_TRUE(state->current().closed);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(state->current().revision, 0u);
    EXPECT_FALSE(engine.events().diagnostics().closed);
    EXPECT_TRUE(engine.events().unregister_and_wait(watch.registration));
}

TEST(runtime_pressure, failed_start) {
    for (const auto mode : std::array{CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        Observations observations;
        CE::Runtime::Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        struct FailingGame final : CE::Game::AbstractGame {
            void init() override { throw std::runtime_error("game initialization failed"); }
            void deinit() override {}
            void update(const CE::Game::TickContext&) override { ADD_FAILURE(); }
            void prepare_render_frame(CE::Rendering::FrameWriter&) const override { ADD_FAILURE(); }
        } game;
        std::shared_ptr<CE::Runtime::Detail::RuntimePressureState> state;
        {
            CE::Runtime::GameRuntime runtime(engine, game, mode);
            state = CE::Runtime::Detail::GameRuntimeAccess::pressure_state(runtime);
            EXPECT_THROW(runtime.run(), std::runtime_error);
            EXPECT_TRUE(state->current().closed);
        }
        EXPECT_TRUE(state->current().closed); // Retained observation state survives its runtime.
        EXPECT_EQ(state->current().revision, 0u);
        EXPECT_FALSE(engine.events().diagnostics().closed);
        EXPECT_EQ(observations.shutdowns, 1);
    }
}

TEST(runtime_pressure, context_routing) {
    PressureFixture first;
    PressureFixture second;
    first.state->bind_owner();
    second.state->bind_owner();
    int first_calls = 0;
    int second_calls = 0;
    const auto first_watch = PressureAccess::watch_pressure(first.runtime, [&](const PressureChange& change) {
        EXPECT_EQ(change.current.source, first.state->current().source);
        ++first_calls;
    });
    const auto second_watch = PressureAccess::watch_pressure(second.runtime, [&](const PressureChange& change) {
        EXPECT_EQ(change.current.source, second.state->current().source);
        ++second_calls;
    });
    first.state->stage_lag(true, 2, 100);
    PressureAccess::publish_pressure(first.runtime);
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 0);
    second.state->stage_lag(true, 3, 200);
    PressureAccess::publish_pressure(second.runtime);
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 1);
    EXPECT_TRUE(first.engine.events().unregister_and_wait(first_watch.registration));
    EXPECT_TRUE(second.engine.events().unregister_and_wait(second_watch.registration));
}

TEST(runtime_pressure, late_watch) {
    PressureFixture fixture;
    fixture.state->bind_owner();
    fixture.state->stage_lag(true, 2, 100);
    PressureAccess::publish_pressure(fixture.runtime);
    std::vector<PressureChange> changes;
    const auto watch = PressureAccess::watch_pressure(fixture.runtime, [&](const PressureChange& change) { changes.push_back(change); });
    EXPECT_TRUE(changes.empty());
    EXPECT_EQ(watch.seed.revision, 1u);
    EXPECT_TRUE(watch.seed.conditions.simulation_lag);
    PressureAccess::publish_pressure(fixture.runtime);
    EXPECT_TRUE(changes.empty());
    fixture.state->stage_lag(false, 2, 100);
    PressureAccess::publish_pressure(fixture.runtime);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes.front().current.revision, 2u);
    EXPECT_TRUE(fixture.engine.events().unregister_and_wait(watch.registration));
    fixture.state->close();
    const auto closed = PressureAccess::watch_pressure(fixture.runtime, [&](const PressureChange&) { ADD_FAILURE(); });
    EXPECT_TRUE(closed.seed.closed);
    EXPECT_EQ(closed.registration.id(), 0u);
    EXPECT_FALSE(fixture.engine.events().diagnostics().closed);
}

TEST(runtime_pressure, reentry) {
    PressureFixture fixture;
    fixture.state->bind_owner();
    std::optional<CE::Runtime::Detail::RuntimePressureWatch> joined;
    std::vector<std::uint64_t> joined_revisions;
    const auto first = PressureAccess::watch_pressure(fixture.runtime, [&](const PressureChange& change) {
        if (!joined) {
            joined = PressureAccess::watch_pressure(fixture.runtime, [&](const PressureChange& next) {
                joined_revisions.push_back(next.current.revision);
            });
            EXPECT_EQ(joined->seed.revision, 1u);
        }
        fixture.state->stage_lag(!change.current.conditions.simulation_lag, 2, 100);
        PressureAccess::publish_pressure(fixture.runtime);
        throw std::runtime_error("optional pressure observer");
    });
    std::vector<std::uint64_t> revisions;
    const auto later = PressureAccess::watch_pressure(fixture.runtime, [&](const PressureChange& change) {
        revisions.push_back(change.current.revision);
    });
    fixture.state->stage_lag(true, 2, 100);
    PressureAccess::publish_pressure(fixture.runtime);
    EXPECT_EQ(revisions, (std::vector<std::uint64_t>{1}));
    EXPECT_TRUE(joined_revisions.empty());
    PressureAccess::publish_pressure(fixture.runtime);
    EXPECT_EQ(revisions, (std::vector<std::uint64_t>{1, 2}));
    EXPECT_EQ(joined_revisions, (std::vector<std::uint64_t>{2}));
    EXPECT_TRUE(fixture.engine.events().unregister_and_wait(first.registration));
    EXPECT_TRUE(fixture.engine.events().unregister_and_wait(later.registration));
    ASSERT_TRUE(joined);
    EXPECT_TRUE(fixture.engine.events().unregister_and_wait(joined->registration));
}

TEST(runtime_pressure, missed_notification) {
    PressureFixture fixture;
    fixture.state->bind_owner();
    fixture.engine.events().close();
    fixture.state->stage_lag(true, 2, 100);
    EXPECT_NO_THROW(PressureAccess::publish_pressure(fixture.runtime));
    const auto current = fixture.state->current();
    EXPECT_EQ(current.revision, 1u);
    EXPECT_TRUE(current.conditions.simulation_lag);
    EXPECT_FALSE(current.closed);
}

TEST(runtime_pressure, watch_failure) {
    PressureFixture fixture;
    fixture.state->bind_owner();
    struct Target {
        std::shared_ptr<CE::Runtime::Detail::RuntimePressureState> state;
        bool& released;

        ~Target() {
            EXPECT_FALSE(state->current().closed);
            released = true;
        }
    };
    bool released = false;
    auto target = std::shared_ptr<Target>(new Target{fixture.state, released});
    CE::Events::EventChannel<PressureChange>::Callback callback = [target](const PressureChange&) {};
    target.reset();
    fixture.engine.events().close();
    EXPECT_THROW(static_cast<void>(PressureAccess::watch_pressure(fixture.runtime, std::move(callback))), CE::Exceptions::failed_operation);
    EXPECT_TRUE(released);
    EXPECT_EQ(fixture.state->current().revision, 0u);
}
