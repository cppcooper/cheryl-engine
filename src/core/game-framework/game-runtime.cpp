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
    GameRuntime::GameRuntime(Engine::EngineContext& engine, AbstractGame& game, const RunMode mode, const Input::PollingOptions polling)
    : engine_(engine), game_(game), mode_(mode), polling_(polling) {
        // Reject an invalid policy before starting any platform or game resources.
        (void)Input::PollingBacklog(polling_);
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
            Input::InputAccumulator accumulator(previous_poll, std::chrono::steady_clock::now());
            Input::PollingBacklog backlog(polling_);

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
                // Detached mailbox work precedes transfer of the complete polling
                // backlog. Posts from that work wait until the next boundary.
                simulation_dispatcher_.drain();
                if (stop_requested_.load(std::memory_order_acquire))
                    break;
                auto state = accumulator.consume_polls(Input::InputClock::now(), backlog.consume());
                game_.update(TickContext{state.elapsed().count(), state, size});
                if (!stop_requested_.load(std::memory_order_acquire))
                    engine_.platform_dispatcher().drain(engine_);

                // One slot is enough because the graphics thread consumes and
                // recycles it before the next simulation update.
                RenderAPIs::RenderFrameWriter writer(frame);
                game_.prepare_render_frame(writer);
                renderer.clear();
                renderer.render(frame);
                engine_.surface().present();
                frame.recycle();
            }
        } catch (...) {
            failure = std::current_exception();
        }

        // Recycle even after a failed prepare/render so frame-held GPU resources
        // are released before the game and its graphics context shut down.
        frame.recycle();
        simulation_dispatcher_.close();
        engine_.platform_dispatcher().close();
        const auto finish = [&failure](auto&& operation) {
            try {
                operation();
            } catch (...) {
                if (!failure)
                    failure = std::current_exception();
            }
        };
        if (game_started)
            finish([this] { game_.deinit(); });
        // Adapters must also clean up if initialize() only completed partway.
        if (input_started)
            finish([&input] { input.deinitialize(); });
        if (renderer_started)
            finish([&renderer] { renderer.deinitialize(); });
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
        constexpr auto cadence = std::chrono::microseconds{16667};
        std::thread worker;
        bool renderer_started = false;
        bool input_started = false;
        bool game_started = false;
        std::exception_ptr failure;

        try {
            renderer_started = true;
            renderer.initialize();
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
                    Input::InputAccumulator accumulator(previous_poll, std::chrono::steady_clock::now());
                    std::vector<std::shared_ptr<const Input::PollSnapshot>> polls;
                    auto next_tick = std::chrono::steady_clock::now() + cadence;
                    while (!stop_requested_.load(std::memory_order_acquire)) {
                        FramebufferSize size;
                        Input::InputClock::time_point consumed_at;
                        {
                            std::unique_lock lock(scheduler_->mutex);
                            scheduler_->wake.wait_until(lock, next_tick, [&] { return stop_requested_.load(std::memory_order_acquire); });
                            if (stop_requested_.load(std::memory_order_acquire))
                                break;
                        }
                        // Run application callbacks without the scheduler lock.
                        // A callback may post platform work or request stop().
                        simulation_dispatcher_.drain();
                        if (stop_requested_.load(std::memory_order_acquire))
                            break;
                        {
                            std::lock_guard lock(scheduler_->mutex);
                            polls = handoff.backlog.consume();
                            size = handoff.framebuffer_size;
                            consumed_at = Input::InputClock::now();
                        }
                        // Capacity becomes available as soon as the entire batch
                        // transfers, even while this worker processes its update.
                        scheduler_->wake.notify_all();
                        // A slow tick never triggers a burst of catch-up updates.
                        // If rendering blocks polling, held input persists without replaying edges.
                        next_tick = std::chrono::steady_clock::now() + cadence;

                        auto state = accumulator.consume_polls(consumed_at, std::move(polls));
                        game_.update(TickContext{state.elapsed().count(), state, size});
                        polls = {}; // Every poll in this batch has been consumed together.
                        if (stop_requested_.load(std::memory_order_acquire))
                            break;

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
                    renderer.clear();
                    renderer.render(slots[*ready].frame);
                    engine_.surface().present();
                    slots[*ready].frame.recycle();
                    {
                        std::lock_guard lock(scheduler_->mutex);
                        slots[*ready].state = SlotState::Free;
                    }
                }

                std::unique_lock lock(scheduler_->mutex);
                const auto deadline = handoff.backlog.next_poll_at();
                scheduler_->wake.wait_until(lock, deadline, [&] {
                    if (stop_requested_.load(std::memory_order_acquire) || handoff.worker_done || handoff.ready ||
                        engine_.platform_dispatcher().has_pending())
                        return true;
                    // Consumption can reopen capacity before spacing has elapsed.
                    // Recompute the deadline instead of waiting on the old full batch.
                    if (handoff.backlog.next_poll_at() != deadline)
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
        engine_.platform_dispatcher().close();
        if (worker.joinable())
            worker.join();
        // Also handles initialization/thread-start failure before owner binding.
        simulation_dispatcher_.close();
        // The worker cannot be writing now. Even incomplete frames must release
        // their handles before deinitializing the game or graphics context.
        const auto finish = [&failure](auto&& operation) {
            try {
                operation();
            } catch (...) {
                if (!failure)
                    failure = std::current_exception();
            }
        };
        for (auto& slot : slots)
            finish([&slot] { slot.frame.recycle(); });
        {
            std::lock_guard lock(scheduler_->mutex);
            if (!failure)
                failure = handoff.worker_failure;
        }
        if (game_started)
            finish([this] { game_.deinit(); });
        if (input_started)
            finish([&input] { input.deinitialize(); });
        if (renderer_started)
            finish([&renderer] { renderer.deinitialize(); });
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
}
