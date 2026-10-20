#include <core/engine/engine-context.h>
#include <core/engine/event-delivery.h>
#include "core/engine/runtime-services-internal.h"
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
#include <future>
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
        int viewport_calls = 0;
        int collections = 0;
        bool pending_retirement = false;
        bool collected_retirement = false;
        bool pending_on_shutdown = false;
        bool fail_viewport = false;
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
        std::chrono::steady_clock::time_point deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        CE::Platform::Dimensions<int> logical_{16, 12};
        CE::Platform::FramebufferSize pixels_{32, 24};
        bool close_ = false;

    public:
        bool consumable = true;
        void request_close() { close_ = true; }
        bool close_requested() const noexcept { return close_; }
        void expire_watchdog() { deadline_ = std::chrono::steady_clock::time_point::min(); }
        CE::Platform::Dimensions<int> logical_size() const override {
            EXPECT_EQ(std::this_thread::get_id(), owner_);
            return logical_;
        }
        CE::Platform::FramebufferSize framebuffer_size() const override {
            EXPECT_EQ(std::this_thread::get_id(), owner_);
            return pixels_;
        }
        CE::Platform::WindowMode mode() const override { return CE::Platform::WindowMode::NORMAL; }
        bool should_close() const override { return close_ || std::chrono::steady_clock::now() >= deadline_; }
        bool supports_close_intent() const noexcept override { return consumable; }
        bool consume_close_intent() override {
            const bool requested = should_close();
            close_ = false;
            return requested;
        }
        void resize(int width, int height) override {
            logical_ = {width, height};
            pixels_ = {2 * width, 2 * height};
        }
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
            observations_.pending_on_shutdown = observations_.pending_retirement;
            ++observations_.shutdowns;
            if (domain_)
                domain_->shutdown();
            publish_graphics_domain_changes();
        }
        void maintain_resources() override {
            ++observations_.collections;
            if (observations_.pending_retirement) {
                observations_.pending_retirement = false;
                observations_.collected_retirement = true;
            }
        }
        void render(const CE::Rendering::Frame& frame) override {
            EXPECT_EQ(frame.passes().size(), 1u);
            ++observations_.renders;
        }
        void clear() override {}
        void set_viewport(CE::Platform::FramebufferSize size) override {
            ++observations_.viewport_calls;
            if (observations_.fail_viewport)
                throw std::runtime_error("viewport apply");
            observations_.viewport = size;
        }
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
        CE::Runtime::ViewAttachment view_;

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
            const auto selected = engine_.simulation_view().snapshot();
            EXPECT_TRUE(selected.initialized);
            EXPECT_EQ(selected.logical_size, tick.logical_size);
            EXPECT_EQ(selected.framebuffer_size, tick.framebuffer_size);
            if (!view_.attached())
                view_ = engine_.simulation_view().watch([selected](const auto& change) {
                    EXPECT_TRUE(change.seed);
                    EXPECT_EQ(change.current, selected);
                });
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

namespace {
    class LifecycleGame final : public CE::Game::AbstractGame {
    public:
        bool fail_init = false;
        bool fail_update = false;
        bool fail_quiesce = false;
        bool fail_deinit = false;
        bool stop_after_update = true;
        int updates = 0;
        int quiesces = 0;
        int deinits = 0;
        std::function<void()> on_update;
        std::function<void()> on_init;
        void init() override {
            if (on_init)
                on_init();
            if (fail_init)
                throw std::runtime_error("game init failure");
        }
        void quiesce() override {
            ++quiesces;
            if (fail_quiesce)
                throw std::runtime_error("game quiesce failure");
        }
        void deinit() override {
            ++deinits;
            if (fail_deinit)
                throw std::runtime_error("game deinit failure");
        }
        void update(const CE::Game::TickContext& tick) override {
            ++updates;
            if (on_update)
                on_update();
            if (fail_update)
                throw std::runtime_error("game update failure");
            if (stop_after_update)
                tick.request_stop();
        }
        void prepare_render_frame(CE::Rendering::FrameWriter& frame) const override {
            static_cast<void>(frame.begin_pass(glm::mat4{1}, glm::mat4{1}));
        }
    };
}

TEST(runtime_services, final_collection_barrier) {
    using namespace CE::Runtime;
    for (const auto mode : std::array{RunMode::Sequential, RunMode::Concurrent}) {
        for (const bool fail : {false, true}) {
            Observations observations;
            Context engine(
                std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
                std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
                std::make_unique<ContractInput>(observations)
            );
            LifecycleGame game;
            GameRuntime runtime(engine, game, mode);
            ServiceAttachment retirement;
            engine.events().register_listener(runtime_ready_event, [&](const auto&) {
                retirement = engine.services().on_maintenance([&](const auto& phase) {
                    if (phase.reason == MaintenanceReason::FinalCollection) {
                        observations.pending_retirement = true;
                        if (fail)
                            throw std::runtime_error("final module failure");
                    }
                });
                runtime.stop();
            });
            if (fail)
                EXPECT_THROW(runtime.run(), std::runtime_error);
            else
                EXPECT_NO_THROW(runtime.run());
            EXPECT_TRUE(observations.collected_retirement);
            EXPECT_FALSE(observations.pending_on_shutdown);
            EXPECT_EQ(observations.shutdowns, 1);
            EXPECT_EQ(game.updates, 0);
            EXPECT_FALSE(retirement.attached());
        }
    }
}

TEST(runtime_services, maintenance_fallback) {
    using namespace CE::Runtime;
    for (const auto mode : std::array{RunMode::Sequential, RunMode::Concurrent}) {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        LifecycleGame game;
        GameRuntime runtime(engine, game, mode);
        ServiceAttachment participant;
        int ordinary = 0;
        int final = 0;
        engine.events().register_listener(runtime_ready_event, [&](const auto&) {
            observations.collections = 0;
            participant = engine.services().on_maintenance([&](const auto& phase) {
                if (phase.reason == MaintenanceReason::FinalCollection)
                    ++final;
                else
                    ++ordinary;
            });
        });
        Detail::RuntimeServicesAccess::before_maintenance(engine.services(), [] { throw std::bad_alloc{}; });
        EXPECT_NO_THROW(runtime.run());
        EXPECT_EQ(final, 1);
        EXPECT_EQ(observations.collections, ordinary + 1); // The final direct barrier runs exactly once.
    }
}

TEST(runtime_services, step_prevents_hook) {
    using namespace CE::Runtime;
    for (const auto mode : std::array{RunMode::Sequential, RunMode::Concurrent}) {
        for (const bool fail : {false, true}) {
            Observations observations;
            Context engine(
                std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
                std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
                std::make_unique<ContractInput>(observations)
            );
            LifecycleGame game;
            GameRuntime runtime(engine, game, mode);
            ServiceAttachment first;
            ServiceAttachment second;
            int entered = 0;
            int later = 0;
            engine.events().register_listener(runtime_ready_event, [&](const auto&) {
                first = engine.services().on_step([&](const auto& phase) {
                    ++entered;
                    EXPECT_EQ(phase.view_revision, engine.simulation_view().snapshot().revision);
                    if (fail)
                        throw std::runtime_error("required step failure");
                    runtime.stop();
                });
                second = engine.services().on_step([&](const auto&) { ++later; });
            }, CE::Events::simulation_delivery(runtime.simulation_dispatcher().submission()), [](std::exception_ptr) {});
            Detail::RuntimeServicesAccess::before_step(engine.services(), [] { throw std::bad_alloc{}; });
            if (fail)
                EXPECT_THROW(runtime.run(), std::runtime_error);
            else
                EXPECT_NO_THROW(runtime.run());
            EXPECT_EQ(entered, 1);
            EXPECT_EQ(later, 1);
            EXPECT_EQ(game.updates, 0);
            EXPECT_EQ(runtime.diagnostics().updates, 0u);
        }
    }
}

TEST(shutdown, default_and_coalescing) {
    using namespace CE::Runtime;
    for (const auto mode : std::array{RunMode::Sequential, RunMode::Concurrent}) {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        LifecycleGame game;
        GameRuntime runtime(engine, game, mode);
        auto endpoint = runtime.shutdown();
        const auto first = endpoint.request();
        EXPECT_EQ(endpoint.request().request.id, first.request.id);
        int requested = 0;
        engine.events().register_listener(shutdown_requested_event, [&](const auto& event) {
            ++requested;
            EXPECT_EQ(event.request.id, first.request.id);
            EXPECT_EQ(event.current.pending_request->id, first.request.id);
            EXPECT_EQ(first.result.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
            throw std::runtime_error("ordinary observer cannot reject shutdown");
        });
        EXPECT_NO_THROW(runtime.run());
        EXPECT_EQ(requested, 1);
        EXPECT_EQ(game.updates, 0);
        EXPECT_EQ(first.result.get().disposition, ShutdownDisposition::Accepted);
        const auto final = runtime.lifecycle().snapshot();
        ASSERT_TRUE(final.committed_request);
        EXPECT_EQ(final.committed_request->id, first.request.id);
        EXPECT_EQ(final.cause, ShutdownCause::AcceptedRequest);
        EXPECT_FALSE(final.reached_ready);
        EXPECT_THROW(static_cast<void>(endpoint.request()), CE::Exceptions::failed_operation);
    }
}

TEST(shutdown, cancel_then_accept) {
    using namespace CE::Runtime;
    for (const auto mode : std::array{RunMode::Sequential, RunMode::Concurrent}) {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        LifecycleGame game;
        game.stop_after_update = false;
        GameRuntime runtime(engine, game, mode);
        auto endpoint = runtime.shutdown();
        ShutdownDecision old_decision;
        int policy_calls = 0;
        std::optional<ShutdownTicket> second;
        auto policy = runtime.set_shutdown_policy([&](const auto&, ShutdownDecision decision) {
            if (++policy_calls == 1)
                old_decision = decision;
            else {
                EXPECT_FALSE(old_decision.accept());
                EXPECT_FALSE(old_decision.cancel());
                EXPECT_TRUE(decision.accept());
            }
        });
        const auto first = endpoint.request();
        engine.events().register_listener(shutdown_request_cancelled_event, [&](const auto& event) {
            EXPECT_EQ(event.request.id, first.request.id);
            EXPECT_EQ(event.reason, ShutdownCancellation::HostRejected);
            second = endpoint.request();
            EXPECT_NE(second->request.id, first.request.id);
        });
        game.on_update = [&] {
            if (game.updates == 1) {
                EXPECT_EQ(endpoint.snapshot().status, ShutdownStatus::Pending);
                EXPECT_TRUE(old_decision.cancel());
            }
            if (game.updates > 20)
                runtime.stop(); // Hang guard, not a required decision latency.
        };
        EXPECT_NO_THROW(runtime.run());
        EXPECT_GE(game.updates, 1);
        EXPECT_EQ(policy_calls, 2);
        EXPECT_EQ(first.result.get().disposition, ShutdownDisposition::Cancelled);
        ASSERT_TRUE(second);
        EXPECT_EQ(second->result.get().disposition, ShutdownDisposition::Accepted);
        EXPECT_FALSE(policy.active());
    }
}

TEST(shutdown, policy_failure_and_withdrawal) {
    using namespace CE::Runtime;
    for (const int scenario : {0, 1, 2}) {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        LifecycleGame game;
        GameRuntime runtime(engine, game);
        ShutdownPolicy policy;
        auto delivery = scenario == 1 ? CE::Events::EventBus::Delivery{[](CE::Events::EventBus::Work) { return false; }} :
            CE::Events::EventBus::Delivery{};
        policy = runtime.set_shutdown_policy([&](const auto&, ShutdownDecision decision) {
            if (scenario == 2) {
                policy.reset(); // Self-withdrawal must not wait for itself.
                EXPECT_FALSE(decision.accept());
            } else
                throw std::runtime_error("host policy failure");
        }, {}, std::move(delivery));
        const auto ticket = runtime.shutdown().request();
        int cancelled = 0;
        engine.events().register_listener(shutdown_request_cancelled_event, [&](const auto& event) {
            ++cancelled;
            EXPECT_EQ(event.reason, scenario == 2 ? ShutdownCancellation::PolicyDetached : ShutdownCancellation::PolicyFailed);
            runtime.stop();
        });
        EXPECT_NO_THROW(runtime.run());
        EXPECT_EQ(cancelled, 1);
        EXPECT_EQ(ticket.result.get().disposition, ShutdownDisposition::Cancelled);
        EXPECT_EQ(runtime.lifecycle().snapshot().outcome, RuntimeOutcome::Completed);
    }
}

TEST(shutdown, late_policy_failure) {
    using namespace CE::Runtime;
    Observations observations;
    Context engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    LifecycleGame game;
    game.stop_after_update = false;
    GameRuntime runtime(engine, game, RunMode::Concurrent);
    auto endpoint = runtime.shutdown();
    std::promise<void> admitted_second;
    auto admitted = admitted_second.get_future();
    int calls = 0; // Only the simulation owner invokes this policy.
    auto policy = runtime.set_shutdown_policy([&](const auto&, ShutdownDecision decision) {
        if (++calls == 1) {
            EXPECT_TRUE(decision.cancel());
            if (admitted.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
                throw std::runtime_error("second admission watchdog");
            throw std::runtime_error("late old policy failure");
        }
        EXPECT_TRUE(decision.accept());
    }, {}, CE::Events::simulation_delivery(runtime.simulation_dispatcher().submission()));
    const auto first = endpoint.request();
    std::optional<ShutdownTicket> second;
    int cancellations = 0;
    engine.events().register_listener(shutdown_request_cancelled_event, [&](const auto&) {
        ++cancellations;
        second = endpoint.request();
    });
    engine.events().register_listener(shutdown_requested_event, [&](const auto& event) {
        if (event.request.id != first.request.id)
            admitted_second.set_value();
    });
    game.on_update = [&] {
        if (calls == 1)
            EXPECT_EQ(endpoint.snapshot().status, ShutdownStatus::Pending);
        if (game.updates > 20)
            runtime.stop();
    };
    EXPECT_NO_THROW(runtime.run());
    EXPECT_EQ(cancellations, 1);
    EXPECT_EQ(calls, 2);
    ASSERT_TRUE(second);
    EXPECT_EQ(first.result.get().disposition, ShutdownDisposition::Cancelled);
    EXPECT_EQ(second->result.get().disposition, ShutdownDisposition::Accepted);
}

TEST(shutdown, native_cancel_and_fallback) {
    using namespace CE::Runtime;
    for (const bool close_bus : {false, true}) {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        auto& window = static_cast<ContractWindow&>(engine.window());
        LifecycleGame game;
        game.on_init = [&] { window.request_close(); };
        GameRuntime runtime(engine, game);
        std::vector<std::uint64_t> intents;
        std::vector<std::uint64_t> requests;
        engine.events().register_listener(CE::Platform::window_close_requested_event, [&](const auto& event) {
            intents.push_back(event.intent);
            if (close_bus)
                engine.events().close(); // Invalidates the ordinary bridge snapshot.
        });
        ShutdownPolicy policy;
        if (!close_bus) {
            policy = runtime.set_shutdown_policy([&](const auto& request, ShutdownDecision decision) {
                requests.push_back(request.id);
                if (requests.size() == 1)
                    EXPECT_TRUE(decision.cancel());
                else
                    EXPECT_TRUE(decision.accept());
            });
            engine.events().register_listener(shutdown_request_cancelled_event, [&](const auto&) { window.request_close(); });
        }
        EXPECT_NO_THROW(runtime.run());
        // Slow teardown diagnostics may outlive the watchdog without renewing native close intent.
        window.expire_watchdog();
        EXPECT_TRUE(window.should_close());
        EXPECT_FALSE(window.close_requested());
        EXPECT_EQ(game.updates, 0);
        if (close_bus) {
            EXPECT_EQ(intents.size(), 1u);
            EXPECT_EQ(runtime.lifecycle().snapshot().cause, ShutdownCause::AcceptedRequest);
        } else {
            ASSERT_EQ(intents.size(), 2u);
            EXPECT_NE(intents[0], intents[1]);
            ASSERT_EQ(requests.size(), 2u);
            EXPECT_NE(requests[0], requests[1]);
        }
    }
}

TEST(shutdown, force_supersedes_pending) {
    using namespace CE::Runtime;
    for (const bool fail : {false, true}) {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        LifecycleGame game;
        game.fail_update = fail;
        GameRuntime runtime(engine, game, RunMode::Concurrent);
        auto policy = runtime.set_shutdown_policy([](const auto&, ShutdownDecision) {});
        const auto ticket = runtime.shutdown().request();
        int cancelled = 0;
        engine.events().register_listener(shutdown_request_cancelled_event, [&](const auto&) { ++cancelled; });
        if (fail)
            EXPECT_THROW(runtime.run(), std::runtime_error);
        else
            EXPECT_NO_THROW(runtime.run());
        EXPECT_EQ(cancelled, 0);
        EXPECT_EQ(ticket.result.get().disposition, ShutdownDisposition::Superseded);
        const auto final = runtime.lifecycle().snapshot();
        ASSERT_TRUE(final.superseded_request);
        EXPECT_EQ(final.superseded_request->id, ticket.request.id);
        EXPECT_EQ(final.cause, fail ? ShutdownCause::Failure : ShutdownCause::StopRequested);
    }
}

TEST(shutdown, withdrawal_waits_for_entry) {
    using namespace CE::Runtime;
    Observations observations;
    Context engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    LifecycleGame game;
    GameRuntime runtime(engine, game, RunMode::Concurrent);
    std::promise<void> entered;
    auto entry = entered.get_future();
    std::promise<void> release;
    auto released = release.get_future();
    std::atomic<bool> in_callback = false;
    auto policy = runtime.set_shutdown_policy([&](const auto&, ShutdownDecision) {
        in_callback = true;
        entered.set_value();
        if (released.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
            throw std::runtime_error("policy withdrawal watchdog");
        in_callback = false;
    }, {}, CE::Events::simulation_delivery(runtime.simulation_dispatcher().submission()));
    const auto ticket = runtime.shutdown().request();
    auto detached = std::async(std::launch::async, [&] {
        if (entry.wait_for(std::chrono::seconds{5}) != std::future_status::ready) {
            ADD_FAILURE() << "Policy entry watchdog";
            return;
        }
        policy.reset();
        EXPECT_FALSE(in_callback.load());
    });
    engine.events().register_listener(shutdown_request_cancelled_event, [&](const auto& event) {
        EXPECT_EQ(event.reason, ShutdownCancellation::PolicyDetached);
        EXPECT_TRUE(in_callback.load());
        release.set_value();
        runtime.stop();
    });
    EXPECT_NO_THROW(runtime.run());
    detached.get();
    EXPECT_EQ(ticket.result.get().disposition, ShutdownDisposition::Cancelled);
}

TEST(shutdown, failed_admission_keeps_running) {
    using namespace CE::Runtime;
    Observations observations;
    Context engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    LifecycleGame game;
    game.on_init = [&] { static_cast<ContractWindow&>(engine.window()).request_close(); };
    GameRuntime runtime(engine, game);
    int policy_calls = 0;
    auto policy = runtime.set_shutdown_policy([&](const auto&, ShutdownDecision) { ++policy_calls; });
    engine.events().register_listener(CE::Platform::window_close_requested_event, [&](const auto&) { engine.events().close(); });
    EXPECT_NO_THROW(runtime.run());
    EXPECT_EQ(game.updates, 1);
    EXPECT_EQ(policy_calls, 0);
    EXPECT_FALSE(runtime.lifecycle().snapshot().pending_request);
    EXPECT_TRUE(runtime.lifecycle().snapshot().reached_ready);
}

TEST(shutdown, forged_close_is_observation) {
    using namespace CE::Runtime;
    Observations observations;
    Context engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    LifecycleGame game;
    GameRuntime runtime(engine, game);
    game.on_init = [&] {
        engine.events().notify(CE::Platform::window_close_requested_event, CE::Platform::WindowCloseRequested{});
        EXPECT_EQ(runtime.shutdown().snapshot().status, ShutdownStatus::Running);
    };
    EXPECT_NO_THROW(runtime.run());
    EXPECT_EQ(game.updates, 1);
    EXPECT_FALSE(runtime.lifecycle().snapshot().committed_request);
}

TEST(shutdown, native_force_supersedes_host_policy) {
    using namespace CE::Runtime;
    for (const auto mode : std::array{RunMode::Sequential, RunMode::Concurrent}) {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        static_cast<ContractWindow&>(engine.window()).consumable = false;
        LifecycleGame game;
        game.stop_after_update = false;
        GameRuntime runtime(engine, game, mode);
        auto policy = runtime.set_shutdown_policy([](const auto&, ShutdownDecision) {}, {.native_close = NativeClosePolicy::Force});
        const auto ticket = runtime.shutdown().request();
        std::future<void> intent;
        game.on_update = [&] {
            if (game.updates == 1)
                intent = engine.platform_dispatcher().submit([](Context& context) {
                    static_cast<ContractWindow&>(context.window()).request_close();
                });
            if (game.updates > 20)
                runtime.stop();
        };
        int cancellations = 0;
        engine.events().register_listener(shutdown_request_cancelled_event, [&](const auto&) { ++cancellations; });
        EXPECT_NO_THROW(runtime.run());
        ASSERT_TRUE(intent.valid());
        EXPECT_NO_THROW(intent.get());
        EXPECT_EQ(cancellations, 0);
        EXPECT_EQ(ticket.result.get().disposition, ShutdownDisposition::Superseded);
        const auto final = runtime.lifecycle().snapshot();
        EXPECT_EQ(final.cause, ShutdownCause::WindowClose);
        ASSERT_TRUE(final.superseded_request);
        EXPECT_EQ(final.superseded_request->id, ticket.request.id);
    }
}

TEST(shutdown, unsupported_and_retained) {
    using namespace CE::Runtime;
    ShutdownEndpoint retained;
    std::optional<ShutdownTicket> ticket;
    {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        static_cast<ContractWindow&>(engine.window()).consumable = false;
        LifecycleGame game;
        GameRuntime runtime(engine, game);
        EXPECT_THROW(static_cast<void>(runtime.set_shutdown_policy([](const auto&, ShutdownDecision) {})), CE::Exceptions::failed_operation);
        auto host_only = runtime.set_shutdown_policy([](const auto&, ShutdownDecision) {}, {.native_close = NativeClosePolicy::Force});
        retained = runtime.shutdown();
        ticket = retained.request();
    }
    EXPECT_TRUE(retained.snapshot().closed);
    ASSERT_TRUE(ticket);
    EXPECT_EQ(ticket->result.get().disposition, ShutdownDisposition::Rejected);
    EXPECT_THROW(static_cast<void>(retained.request()), CE::Exceptions::failed_operation);
}

TEST(runtime_lifecycle, ready_and_owner_gate) {
    using namespace CE::Runtime;
    for (const auto mode : std::array{RunMode::Sequential, RunMode::Concurrent}) {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        LifecycleGame game;
        GameRuntime runtime(engine, game, mode);
        const auto platform = std::this_thread::get_id();
        std::vector<RuntimeStage> stages;
        std::uint64_t session = 0;
        bool simulation_ready = false;
        engine.events().register_listener(runtime_ready_event, [&](const auto& event) {
            EXPECT_EQ(std::this_thread::get_id(), platform);
            EXPECT_TRUE(event.current.initialized.renderer);
            EXPECT_TRUE(event.current.initialized.input);
            EXPECT_TRUE(event.current.initialized.game);
            EXPECT_TRUE(event.current.initialized.simulation_owner);
            EXPECT_TRUE(engine.simulation_view().snapshot().initialized);
            EXPECT_EQ(game.updates, 0);
            session = event.current.session;
            stages.push_back(event.current.stage);
            throw std::runtime_error("optional Ready observer");
        });
        engine.events().register_listener(runtime_ready_event, [&](const auto&) {
            simulation_ready = true;
            EXPECT_EQ(std::this_thread::get_id() == platform, mode == RunMode::Sequential);
        }, CE::Events::simulation_delivery(runtime.simulation_dispatcher().submission()), [](std::exception_ptr) {});
        game.on_update = [&] { EXPECT_TRUE(simulation_ready); };
        engine.events().register_listener(shutdown_starting_event, [&](const auto& event) {
            EXPECT_EQ(event.current.session, session);
            EXPECT_EQ(std::this_thread::get_id(), platform);
            EXPECT_EQ(event.current.cause, ShutdownCause::StopRequested);
            stages.push_back(event.current.stage);
        });
        engine.events().register_listener(runtime_quiescing_event, [&](const auto& event) {
            EXPECT_EQ(game.quiesces, 1);
            EXPECT_EQ(game.deinits, 0);
            stages.push_back(event.current.stage);
        });
        engine.events().register_listener(runtime_stopped_event, [&](const auto& event) {
            EXPECT_EQ(game.deinits, 1);
            EXPECT_EQ(observations.shutdowns, 1);
            EXPECT_EQ(event.current.outcome, RuntimeOutcome::Completed);
            EXPECT_EQ(event.current.stats.updates, 1u);
            EXPECT_EQ(event.current.sequence, 4u);
            stages.push_back(event.current.stage);
            throw std::runtime_error("optional final observer");
        });
        EXPECT_NO_THROW(runtime.run());
        EXPECT_EQ(stages, (std::vector{RuntimeStage::Ready, RuntimeStage::Stopping, RuntimeStage::Quiescing, RuntimeStage::Stopped}));
        EXPECT_TRUE(runtime.lifecycle().snapshot().closed);
        const auto events = stages.size();
        GameRuntime rejected(engine, game, mode);
        EXPECT_THROW(rejected.run(), CE::Exceptions::failed_operation);
        EXPECT_EQ(stages.size(), events);
        EXPECT_EQ(rejected.lifecycle().snapshot().stage, RuntimeStage::Unstarted);
    }
}

TEST(runtime_lifecycle, partial_start_and_pre_stop) {
    using namespace CE::Runtime;
    for (const auto mode : std::array{RunMode::Sequential, RunMode::Concurrent}) {
        for (const bool pre_stop : {false, true}) {
            Observations observations;
            Context engine(
                std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
                std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
                std::make_unique<ContractInput>(observations)
            );
            LifecycleGame game;
            game.fail_init = !pre_stop;
            GameRuntime runtime(engine, game, mode);
            int ready = 0;
            int starting = 0;
            int quiescing = 0;
            int stopped = 0;
            engine.events().register_listener(runtime_ready_event, [&](const auto&) { ++ready; });
            engine.events().register_listener(shutdown_starting_event, [&](const auto& event) {
                ++starting;
                EXPECT_FALSE(event.current.reached_ready);
                EXPECT_FALSE(event.current.initialized.game);
                EXPECT_EQ(event.current.initialized.renderer, !pre_stop);
            });
            engine.events().register_listener(runtime_quiescing_event, [&](const auto&) { ++quiescing; });
            engine.events().register_listener(runtime_stopped_event, [&](const auto& event) {
                ++stopped;
                EXPECT_EQ(event.current.sequence, 3u);
                EXPECT_NE(event.current.context, 0u);
                EXPECT_NE(event.current.session, 0u);
                EXPECT_EQ(event.current.outcome, pre_stop ? RuntimeOutcome::Completed : RuntimeOutcome::Failed);
            });
            if (pre_stop) {
                runtime.stop();
                EXPECT_NO_THROW(runtime.run());
            } else {
                EXPECT_THROW(runtime.run(), std::runtime_error);
            }
            EXPECT_EQ(ready, 0);
            EXPECT_EQ(starting, 1);
            EXPECT_EQ(quiescing, 1);
            EXPECT_EQ(stopped, 1);
            EXPECT_EQ(game.updates, 0);
        }
    }
}

TEST(runtime_lifecycle, preserved_failures) {
    using namespace CE::Runtime;
    std::optional<RuntimeLifecycle> retained;
    for (const auto mode : std::array{RunMode::Sequential, RunMode::Concurrent}) {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        LifecycleGame game;
        game.fail_update = game.fail_quiesce = game.fail_deinit = true;
        GameRuntime runtime(engine, game, mode);
        retained = runtime.lifecycle();
        try {
            runtime.run();
            ADD_FAILURE() << "Update failure must escape";
        } catch (const std::runtime_error& error) {
            EXPECT_STREQ(error.what(), "game update failure");
        }
        const auto final = retained->snapshot();
        EXPECT_EQ(final.outcome, RuntimeOutcome::Failed);
        EXPECT_EQ(final.primary_failure.message_text(), "game update failure");
        EXPECT_EQ(final.primary_failure.phase_text(), "game_update");
        ASSERT_EQ(final.secondary_failure_count, 2u);
        EXPECT_EQ(final.secondary_failures[0].message_text(), "game quiesce failure");
        EXPECT_EQ(final.secondary_failures[1].message_text(), "game deinit failure");
        EXPECT_EQ(observations.shutdowns, 1);
    }
    ASSERT_TRUE(retained);
    EXPECT_EQ(retained->snapshot().primary_failure.message_text(), "game update failure");
}

TEST(runtime_lifecycle, stop_at_ready) {
    using namespace CE::Runtime;
    for (const auto mode : std::array{RunMode::Sequential, RunMode::Concurrent}) {
        Observations observations;
        Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        LifecycleGame game;
        GameRuntime runtime(engine, game, mode);
        engine.events().register_listener(runtime_ready_event, [&](const auto&) { runtime.stop(); });
        runtime.run();
        EXPECT_EQ(game.updates, 0);
        EXPECT_TRUE(runtime.lifecycle().snapshot().reached_ready);
    }
}

TEST(runtime_lifecycle, worker_start_failure) {
    using namespace CE::Runtime;
    Observations observations;
    Context engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    LifecycleGame game;
    GameRuntime runtime(engine, game, RunMode::Concurrent);
    Detail::GameRuntimeAccess::set_simulation_thread_factory(runtime, [](std::function<void()>) -> std::thread {
        throw std::runtime_error("thread creation");
    });
    EXPECT_THROW(runtime.run(), std::runtime_error);
    const auto final = runtime.lifecycle().snapshot();
    EXPECT_FALSE(final.reached_ready);
    EXPECT_FALSE(final.initialized.simulation_owner);
    EXPECT_EQ(final.primary_failure.message_text(), "thread creation");
    EXPECT_EQ(final.stage, RuntimeStage::Stopped);
}

TEST(runtime_lifecycle, bootstrap_platform_progress) {
    using namespace CE::Runtime;
    Observations observations;
    Context engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    LifecycleGame game;
    GameRuntime runtime(engine, game, RunMode::Concurrent);
    const auto platform = std::this_thread::get_id();
    std::atomic<bool> serviced = false;
    engine.events().register_listener(simulation_view_changed_event, [&](const auto&) {
        auto result = engine.platform_dispatcher().submit([&](Context&) {
            EXPECT_EQ(std::this_thread::get_id(), platform);
            serviced = true;
        });
        ASSERT_EQ(result.wait_for(std::chrono::seconds{5}), std::future_status::ready);
        result.get();
    });
    engine.events().register_listener(runtime_ready_event, [&](const auto&) { EXPECT_TRUE(serviced.load()); });
    EXPECT_NO_THROW(runtime.run());
    EXPECT_EQ(game.updates, 1);
}

TEST(runtime_lifecycle, starting_before_hook_returns) {
    using namespace CE::Runtime;
    Observations observations;
    Context engine(
        std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
        std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
        std::make_unique<ContractInput>(observations)
    );
    LifecycleGame game;
    GameRuntime runtime(engine, game, RunMode::Concurrent);
    std::promise<void> starting;
    auto started = starting.get_future();
    std::atomic<bool> in_hook = false;
    engine.events().register_listener(shutdown_starting_event, [&](const auto&) {
        EXPECT_TRUE(in_hook.load());
        starting.set_value();
    });
    engine.events().register_listener(runtime_quiescing_event, [&](const auto&) { EXPECT_FALSE(in_hook.load()); });
    game.on_update = [&] {
        in_hook = true;
        auto stop = engine.platform_dispatcher().submit([&](Context&) { runtime.stop(); });
        if (stop.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
            throw std::runtime_error("platform stop watchdog");
        stop.get();
        if (started.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
            throw std::runtime_error("Starting watchdog");
        in_hook = false;
    };
    EXPECT_NO_THROW(runtime.run());
    EXPECT_EQ(game.updates, 1);
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

TEST(runtime_view, window_selection) {
    for (const auto mode : std::array{CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        Observations observations;
        CE::Runtime::Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        std::vector<CE::Runtime::WindowState> windows;
        engine.events().register_listener(CE::Runtime::window_state_changed_event, [&](const auto& change) {
            windows.push_back(change.current);
            if (windows.size() == 1)
                engine.window().resize(40, 30); // Fresh selection must follow after fanout.
            throw std::runtime_error("optional window observer");
        });
        class ViewGame final : public CE::Game::AbstractGame {
            CE::Runtime::Context& engine_;
        public:
            explicit ViewGame(CE::Runtime::Context& engine) : engine_(engine) {}
            void init() override {}
            void quiesce() override {}
            void deinit() override {}
            void update(const CE::Game::TickContext& tick) override {
                const auto selected = engine_.simulation_view().snapshot();
                EXPECT_EQ(selected.logical_size, tick.logical_size);
                EXPECT_EQ(selected.framebuffer_size, tick.framebuffer_size);
                if (tick.logical_size.width == 40)
                    tick.request_stop();
            }
            void prepare_render_frame(CE::Rendering::FrameWriter& frame) const override {
                static_cast<void>(frame.begin_pass(glm::mat4{1}, glm::mat4{1}));
            }
        } game(engine);
        CE::Runtime::GameRuntime runtime(engine, game, mode);
        runtime.run();
        ASSERT_EQ(windows.size(), 2u);
        EXPECT_EQ(windows[0].framebuffer_size, (CE::Platform::FramebufferSize{32, 24}));
        EXPECT_EQ(windows[1].logical_size, (CE::Platform::Dimensions<int>{40, 30}));
        EXPECT_EQ(windows[1].framebuffer_size, (CE::Platform::FramebufferSize{80, 60}));
        EXPECT_EQ(observations.viewport_calls, 2);
        EXPECT_EQ(observations.viewport, (CE::Platform::FramebufferSize{80, 60}));
        EXPECT_TRUE(engine.simulation_view().snapshot().closed);
    }
}

TEST(runtime_view, failed_required_seed) {
    for (const auto mode : std::array{CE::Runtime::RunMode::Sequential, CE::Runtime::RunMode::Concurrent}) {
        Observations observations;
        observations.fail_viewport = true;
        CE::Runtime::Context engine(
            std::make_unique<ContractDisplay>(), std::make_unique<ContractSurface>(observations),
            std::make_unique<ContractRenderer>(observations), std::make_unique<ContractResources>(),
            std::make_unique<ContractInput>(observations)
        );
        ContractGame game(engine, observations);
        // Quiesce expects a stop source only after update in the ordinary fixture.
        observations.saved_stop = std::stop_source{};
        observations.saved_stop.request_stop();
        CE::Runtime::GameRuntime runtime(engine, game, mode);
        EXPECT_THROW(runtime.run(), std::runtime_error);
        EXPECT_EQ(observations.viewport_calls, 1);
        EXPECT_EQ(observations.updates, 0);
        EXPECT_EQ(observations.renders, 0);
        EXPECT_EQ(observations.shutdowns, 1);
        EXPECT_EQ(observations.game_shutdowns, 1);
    }
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
