#include <core/game-framework/game-runtime.h>

#include <core/controls/input-accumulator.h>
#include <core/controls/input-interface.h>
#include <core/display/window-interface.h>
#include <core/engine/engine-context.h>
#include <core/game-framework/abstract-game.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <internals/exceptions.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace CE::GFramework {
    GameRuntime::GameRuntime(
        Engine::EngineContext& engine,
        AbstractGame& game,
        const RunMode mode,
        const Input::PollingOptions polling,
        const SimulationTimingOptions timing
    )
    : engine_(engine), game_(game), mode_(mode), polling_(polling), timing_(timing) {
        // Reject an invalid policy before starting any platform or game resources.
        (void)Input::PollingBacklog(polling_);
        (void)SimulationScheduler(timing_);
        if (mode_ != RunMode::Sequential && mode_ != RunMode::Concurrent)
            throw Exceptions::invalid_args(CE_HERE, "Unknown game runtime mode");
    }

    void GameRuntime::run() {
        if (run_started_.exchange(true, std::memory_order_acq_rel))
            throw Exceptions::failed_operation(CE_HERE, "GameRuntime::run is single-use");
        // Reserve the graph before either runtime can initialize or clean up its adapters.
        engine_.begin_session();
        try {
            if (mode_ == RunMode::Sequential)
                run_sequential();
            else
                run_concurrent();
        } catch (...) {
            stop();
            throw;
        }
        stop();
    }

    void GameRuntime::run_sequential() {
        if (stop_requested_.load(std::memory_order_acquire))
            return;

        auto& window = engine_.window();
        auto& renderer = engine_.renderer();
        auto& input = engine_.input();
        RenderAPIs::RenderFrame frame;
        bool renderer_started = false;
        bool input_started = false;
        bool game_started = false;
        std::exception_ptr failure;

        try {
            renderer_started = true;
            renderer.initialize();
            renderer_ready_ = true;
            engine_.platform_dispatcher().open([scheduler = scheduler_] {
                std::lock_guard lock(scheduler->mutex);
                scheduler->wake.notify_all();
            });
            simulation_dispatcher_.open([scheduler = scheduler_] {
                std::lock_guard lock(scheduler->mutex);
                scheduler->wake.notify_all();
            });
            simulation_dispatcher_.bind_owner();
            input_started = true;
            input.initialize(window);
            game_started = true;
            game_.init();
            if (!stop_requested_.load(std::memory_order_acquire))
                engine_.platform_dispatcher().drain(engine_);

            // Game initialization may register bindings and upload assets. Start
            // timing and sample the baseline only after it has completed.
            auto previous_poll = input.action_snapshot();
            if (!previous_poll)
                throw Exceptions::failed_operation(CE_HERE, "Input adapter did not supply an initial snapshot");
            auto viewport = window.framebuffer_size();
            renderer.set_viewport(viewport);
            const auto started_at = SimulationClock::now();
            Input::InputAccumulator accumulator(previous_poll, started_at);
            Input::PollingBacklog backlog(polling_);
            SimulationScheduler timing(timing_, started_at);
            bool published = false;

            while (!stop_requested_.load(std::memory_order_acquire) && !window.should_close()) {
                engine_.platform_dispatcher().drain(engine_);
                // Sequential execution cannot poll during update(), but spacing
                // still applies. A delayed poll never delays simulation or rendering.
                if (backlog.poll_due(Input::InputClock::now())) {
                    input.poll();
                    if (stop_requested_.load(std::memory_order_acquire) || window.should_close())
                        break;
                    backlog.complete(input.poll_snapshot(), Input::InputClock::now());
                }
                if (stop_requested_.load(std::memory_order_acquire) || window.should_close())
                    break;
                const auto size = window.framebuffer_size();
                if (size != viewport) {
                    renderer.set_viewport(size);
                    viewport = size;
                }
                const auto batch = timing.advance(SimulationClock::now());
                bool updated = false;
                for (std::size_t i = 0; i < batch.steps.size(); ++i) {
                    // Every actual update gets its own detached mailbox and whole
                    // input transfer. A cycle with no update leaves the backlog intact.
                    simulation_dispatcher_.drain();
                    if (stop_requested_.load(std::memory_order_acquire))
                        break;
                    auto state = accumulator.consume_polls(Input::InputClock::now(), backlog.consume());
                    const auto& step = batch.steps[i];
                    const auto dropped = i + 1 == batch.steps.size() ? std::chrono::duration<double>(batch.dropped).count() : 0.0;
                    game_.update(TickContext{std::chrono::duration<double>(step.delta).count(), state, size, step.kind, dropped});
                    updated = true;
                    if (stop_requested_.load(std::memory_order_acquire))
                        break;
                }
                if (!stop_requested_.load(std::memory_order_acquire))
                    engine_.platform_dispatcher().drain(engine_);

                // Publish only the final useful state from the bounded batch.
                // Retain that complete frame for cycles without a simulation update.
                if (updated) {
                    frame.recycle();
                    RenderAPIs::RenderFrameWriter writer(frame);
                    game_.prepare_render_frame(writer);
                    published = true;
                }
                if (published) {
                    renderer.clear();
                    renderer.render(frame);
                    engine_.surface().present();
                }
                renderer.maintain_resources();
                std::unique_lock lock(scheduler_->mutex);
                const auto deadline = std::min({timing.next_update_at(), backlog.next_poll_at(),
                    SimulationClock::now() + resource_maintenance_interval});
                scheduler_->wake.wait_until(lock, deadline, [&] {
                    return stop_requested_.load(std::memory_order_acquire) || engine_.platform_dispatcher().has_pending();
                });
            }
        } catch (...) {
            failure = std::current_exception();
        }

        const auto finish = [&failure](auto&& operation) {
            try {
                operation();
            } catch (...) {
                if (!failure)
                    failure = std::current_exception();
            }
        };
        stop();
        finish([this] { engine_.close_worker_submissions(); });
        finish([this] { simulation_dispatcher_.close(); });
        if (game_started)
            finish([this] { game_.quiesce(); });
        finish_worker_shutdown(failure);
        finish([this] { engine_.platform_dispatcher().close(); });
        // Frames and game dependencies remain alive through accepted CPU work.
        finish([&frame] { frame.recycle(); });
        if (game_started)
            finish([this] { game_.deinit(); });
        if (renderer_ready_)
            finish([&renderer] { renderer.maintain_resources(); });
        // Adapters must also clean up if initialize() only completed partway.
        if (input_started)
            finish([&input] { input.deinitialize(); });
        if (renderer_started)
            finish([&renderer] { renderer.deinitialize(); });
        renderer_ready_ = false;
        if (failure)
            std::rethrow_exception(failure);
    }

    void GameRuntime::run_concurrent() {
        if (stop_requested_.load(std::memory_order_acquire))
            return;

        enum class SlotState { Free, Writing, Ready, Rendering, Retired, Recycling };
        struct Slot {
            RenderAPIs::RenderFrame frame;
            SlotState state = SlotState::Free; // Guarded by scheduler_->mutex.
        };
        struct Handoff {
            Input::PollingBacklog backlog;
            FramebufferSize framebuffer_size;
            std::optional<std::size_t> ready;
            std::exception_ptr worker_failure;
            bool worker_done = false;
        };

        auto& window = engine_.window();
        auto& renderer = engine_.renderer();
        auto& input = engine_.input();
        std::array<Slot, 3> slots;
        Handoff handoff{Input::PollingBacklog(polling_)};
        std::thread worker;
        bool renderer_started = false;
        bool input_started = false;
        bool game_started = false;
        std::exception_ptr failure;

        try {
            renderer_started = true;
            renderer.initialize();
            renderer_ready_ = true;
            engine_.platform_dispatcher().open([scheduler = scheduler_] {
                std::lock_guard lock(scheduler->mutex);
                scheduler->wake.notify_all();
            });
            // Accept initialization-time posts before the dedicated simulation
            // worker exists; execution ownership is bound by that worker.
            simulation_dispatcher_.open([scheduler = scheduler_] {
                std::lock_guard lock(scheduler->mutex);
                scheduler->wake.notify_all();
            });
            input_started = true;
            input.initialize(window);
            game_started = true;
            game_.init();
            if (!stop_requested_.load(std::memory_order_acquire))
                engine_.platform_dispatcher().drain(engine_);

            auto previous_poll = input.action_snapshot();
            if (!previous_poll)
                throw Exceptions::failed_operation(CE_HERE, "Input adapter did not supply an initial snapshot");
            auto viewport = window.framebuffer_size();
            renderer.set_viewport(viewport);
            handoff.framebuffer_size = viewport;

            // The worker owns simulation and its clock; the calling thread alone
            // touches the window, input adapter, renderer, and presentation surface.
            worker = std::thread([&, previous_poll = std::move(previous_poll)]() mutable {
                try {
                    simulation_dispatcher_.bind_owner();
                    const auto started_at = SimulationClock::now();
                    Input::InputAccumulator accumulator(previous_poll, started_at);
                    SimulationScheduler timing(timing_, started_at);
                    std::vector<std::shared_ptr<const Input::PollSnapshot>> polls;
                    while (!stop_requested_.load(std::memory_order_acquire)) {
                        {
                            std::unique_lock lock(scheduler_->mutex);
                            scheduler_->wake.wait_until(lock, timing.next_update_at(), [&] {
                                return stop_requested_.load(std::memory_order_acquire);
                            });
                            if (stop_requested_.load(std::memory_order_acquire))
                                break;
                        }
                        const auto batch = timing.advance(SimulationClock::now());
                        bool updated = false;
                        for (std::size_t i = 0; i < batch.steps.size(); ++i) {
                            // Callbacks run outside the scheduler lock. Each recovery
                            // update consumes fresh polls or persistent State, never
                            // a replay of the previous update's edges/Events/Text.
                            simulation_dispatcher_.drain();
                            if (stop_requested_.load(std::memory_order_acquire))
                                break;
                            FramebufferSize size;
                            Input::InputClock::time_point consumed_at;
                            {
                                std::lock_guard lock(scheduler_->mutex);
                                polls = handoff.backlog.consume();
                                size = handoff.framebuffer_size;
                                consumed_at = Input::InputClock::now();
                            }
                            scheduler_->wake.notify_all();
                            auto state = accumulator.consume_polls(consumed_at, std::move(polls));
                            const auto& step = batch.steps[i];
                            const auto dropped = i + 1 == batch.steps.size() ? std::chrono::duration<double>(batch.dropped).count() : 0.0;
                            game_.update(TickContext{std::chrono::duration<double>(step.delta).count(), state, size, step.kind, dropped});
                            polls = {};
                            updated = true;
                            if (stop_requested_.load(std::memory_order_acquire))
                                break;
                        }
                        if (stop_requested_.load(std::memory_order_acquire))
                            break;
                        if (!updated)
                            continue;

                        // Never wait for the renderer to release a slot. Simulation
                        // keeps updating; only a visual snapshot is skipped.
                        std::optional<std::size_t> writable;
                        {
                            std::lock_guard lock(scheduler_->mutex);
                            for (std::size_t i = 0; i < slots.size(); ++i) {
                                if (slots[i].state == SlotState::Free) {
                                    slots[i].state = SlotState::Writing;
                                    writable = i;
                                    break;
                                }
                            }
                        }
                        if (!writable)
                            continue;

                        RenderAPIs::RenderFrameWriter writer(slots[*writable].frame);
                        game_.prepare_render_frame(writer);
                        {
                            std::lock_guard lock(scheduler_->mutex);
                            if (handoff.ready)
                                slots[*handoff.ready].state = SlotState::Retired;
                            slots[*writable].state = SlotState::Ready;
                            handoff.ready = *writable;
                        }
                        scheduler_->wake.notify_all();
                    }
                } catch (...) {
                    std::lock_guard lock(scheduler_->mutex);
                    handoff.worker_failure = std::current_exception();
                }
                // Release unexecuted simulation captures on their owner while
                // game/resources still exist, before publishing worker_done.
                try {
                    simulation_dispatcher_.close();
                } catch (...) {
                    std::lock_guard lock(scheduler_->mutex);
                    if (!handoff.worker_failure)
                        handoff.worker_failure = std::current_exception();
                }
                {
                    std::lock_guard lock(scheduler_->mutex);
                    handoff.worker_done = true;
                }
                scheduler_->wake.notify_all();
            });

            std::optional<std::size_t> current_frame;
            // Full batches pause only polling. Rendering and recycling remain
            // available, and consumption wakes the platform to resume polling.
            while (!stop_requested_.load(std::memory_order_acquire) && !window.should_close()) {
                engine_.platform_dispatcher().drain(engine_);
                bool poll_due = false;
                {
                    std::lock_guard lock(scheduler_->mutex);
                    if (handoff.worker_done)
                        break;
                    poll_due = handoff.backlog.poll_due(Input::InputClock::now());
                }

                if (poll_due) {
                    input.poll();
                    if (stop_requested_.load(std::memory_order_acquire) || window.should_close())
                        break;
                    auto completed = input.poll_snapshot();
                    if (!completed)
                        throw Exceptions::failed_operation(CE_HERE, "Input adapter did not publish a snapshot");
                    const auto size = window.framebuffer_size();
                    if (size != viewport) {
                        renderer.set_viewport(size);
                        viewport = size;
                    }
                    {
                        std::lock_guard lock(scheduler_->mutex);
                        // Every completed poll consumes capacity, including an
                        // unchanged observation. No completed observation is discarded.
                        handoff.backlog.complete(std::move(completed), Input::InputClock::now());
                        handoff.framebuffer_size = size;
                    }
                }

                // Superseded frames still hold asset handles. Only this thread
                // may recycle them, while the graphics context is current.
                for (auto& slot : slots) {
                    {
                        std::lock_guard lock(scheduler_->mutex);
                        if (slot.state != SlotState::Retired)
                            continue;
                        slot.state = SlotState::Recycling;
                    }
                    slot.frame.recycle();
                    {
                        std::lock_guard lock(scheduler_->mutex);
                        slot.state = SlotState::Free;
                    }
                }

                std::optional<std::size_t> ready;
                {
                    std::lock_guard lock(scheduler_->mutex);
                    ready = std::exchange(handoff.ready, std::nullopt);
                    if (ready)
                        slots[*ready].state = SlotState::Rendering;
                }
                if (ready) {
                    if (current_frame) {
                        slots[*current_frame].frame.recycle();
                        std::lock_guard lock(scheduler_->mutex);
                        slots[*current_frame].state = SlotState::Free;
                    }
                    current_frame = ready;
                }
                if (current_frame) {
                    renderer.clear();
                    renderer.render(slots[*current_frame].frame);
                    engine_.surface().present();
                }
                // Retirement must progress even before the first frame, while
                // input capacity is full, or while a slow update produces no frame.
                renderer.maintain_resources();

                std::unique_lock lock(scheduler_->mutex);
                const auto poll_deadline = handoff.backlog.next_poll_at();
                const auto deadline = std::min(poll_deadline, SimulationClock::now() + resource_maintenance_interval);
                scheduler_->wake.wait_until(lock, deadline, [&] {
                    if (stop_requested_.load(std::memory_order_acquire) || handoff.worker_done || handoff.ready ||
                        engine_.platform_dispatcher().has_pending())
                        return true;
                    // Consumption can reopen capacity before spacing has elapsed.
                    // Recompute the deadline instead of waiting on the old full batch.
                    if (handoff.backlog.next_poll_at() != poll_deadline)
                        return true;
                    for (const auto& slot : slots)
                        if (slot.state == SlotState::Retired)
                            return true;
                    return false;
                });
            }
        } catch (...) {
            failure = std::current_exception();
        }

        stop();
        const auto finish = [&failure](auto&& operation) {
            try {
                operation();
            } catch (...) {
                if (!failure)
                    failure = std::current_exception();
            }
        };
        finish([this] { engine_.close_worker_submissions(); });
        if (worker.joinable()) {
            // A finishing simulation callback may depend on platform completion.
            // Wait on published completion while still servicing that owner.
            while (true) {
                {
                    std::lock_guard lock(scheduler_->mutex);
                    if (handoff.worker_done)
                        break;
                }
                pump_shutdown_requests(failure);
                std::unique_lock lock(scheduler_->mutex);
                scheduler_->wake.wait_for(lock, std::chrono::milliseconds{1}, [&] {
                    return handoff.worker_done || engine_.platform_dispatcher().has_pending();
                });
            }
            worker.join();
        }
        {
            std::lock_guard lock(scheduler_->mutex);
            if (!failure)
                failure = handoff.worker_failure;
        }
        // Also handles initialization/thread-start failure before owner binding.
        finish([this] { simulation_dispatcher_.close(); });
        if (game_started)
            finish([this] { game_.quiesce(); });
        finish_worker_shutdown(failure);
        finish([this] { engine_.platform_dispatcher().close(); });
        for (auto& slot : slots)
            finish([&slot] { slot.frame.recycle(); });
        if (game_started)
            finish([this] { game_.deinit(); });
        if (renderer_ready_)
            finish([&renderer] { renderer.maintain_resources(); });
        if (input_started)
            finish([&input] { input.deinitialize(); });
        if (renderer_started)
            finish([&renderer] { renderer.deinitialize(); });
        renderer_ready_ = false;
        if (failure)
            std::rethrow_exception(failure);
    }

    void GameRuntime::stop() {
        {
            std::lock_guard lock(scheduler_->mutex);
            stop_requested_.store(true, std::memory_order_release);
        }
        scheduler_->wake.notify_all();
    }

    void GameRuntime::pump_shutdown_requests(std::exception_ptr& failure) {
        try {
            engine_.platform_dispatcher().drain(engine_);
        }
        catch (...) {
            if (!failure)
                failure = std::current_exception();
            // If dispatch itself fails, cancel rather than strand futures which
            // a worker is waiting for. Preserve the first failure during cleanup.
            try {
                engine_.platform_dispatcher().close();
            }
            catch (...) {
                if (!failure)
                    failure = std::current_exception();
            }
        }
        // Maintenance failure is separate from dispatcher failure: keep servicing
        // accepted CPU-to-platform completions while retaining the original error.
        if (renderer_ready_) {
            try {
                engine_.renderer().maintain_resources();
            } catch (...) {
                if (!failure)
                    failure = std::current_exception();
            }
        }
    }

    void GameRuntime::finish_worker_shutdown(std::exception_ptr& failure) {
        while (!engine_.workers_idle()) {
            pump_shutdown_requests(failure);
            std::unique_lock lock(scheduler_->mutex);
            // Worker accounting has no borrowed runtime wake callback. A bounded
            // wait observes completion while platform posts wake immediately.
            scheduler_->wake.wait_for(lock, std::chrono::milliseconds{1}, [this] {
                return engine_.platform_dispatcher().has_pending();
            });
        }
        try {
            engine_.finish_workers();
        }
        catch (...) {
            if (!failure)
                failure = std::current_exception();
        }
    }
}
