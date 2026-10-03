#include <core/game-framework/game-runtime.h>
#include "game-runtime-internal.h"

#include <core/controls/input-accumulator.h>
#include <core/controls/input-interface.h>
#include <core/display/window-interface.h>
#include <core/engine/engine-context.h>
#include <core/game-framework/abstract-game.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <internals/exceptions.h>
#include <internals/failure-reporting.h>
#include <internals/compile-time-logging.hpp>

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
    void RuntimeDetail::GameRuntimeAccess::set_simulation_thread_factory(
        GameRuntime& runtime,
        std::function<std::thread(std::function<void()>)> factory
    ) {
        if (!factory)
            throw Exceptions::invalid_args(CE_HERE, "Simulation thread factory must not be empty");
        if (runtime.run_started_.load(std::memory_order_acquire))
            throw Exceptions::failed_operation(CE_HERE, "Simulation thread factory must be configured before run");
        runtime.simulation_thread_factory_ = std::move(factory);
    }

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
        try {
            engine_.begin_session();
            CE_LOG_INFO(CE::enginelog, "subsystem=runtime domain={} context={} operation=session_begin mode={} timing={} polling={} capacity={}",
                        diagnostics_.domain, engine_.diagnostic_id(), static_cast<int>(mode_), static_cast<int>(timing_.mode),
                        static_cast<int>(polling_.policy), polling_.capacity);
            if (mode_ == RunMode::Sequential)
                run_sequential();
            else
                run_concurrent();
        } catch (...) {
            stop();
            report_session(true);
            run_finished_.store(true, std::memory_order_release);
            throw;
        }
        stop();
        report_session(false);
        run_finished_.store(true, std::memory_order_release);
    }

    RuntimeStats GameRuntime::diagnostics() const {
        if (run_started_.load(std::memory_order_acquire) && !run_finished_.load(std::memory_order_acquire))
            throw Exceptions::failed_operation(CE_HERE, "Runtime diagnostics require a completed or unstarted session");
        return diagnostics_;
    }

    void GameRuntime::report_session(const bool failed) const noexcept {
        if (failed) {
            Diagnostics::report_outcome("runtime", diagnostics_.domain, phase_, "failed");
            CE_LOG_ERROR(CE::enginelog, "subsystem=runtime domain={} operation={} outcome=failed", diagnostics_.domain, phase_);
        }
        CE_LOG_INFO(CE::enginelog, "subsystem=runtime domain={} operation=session_end outcome={} updates={} polls={} published={} rendered={}",
                    diagnostics_.domain, failed ? "failed" : "completed", diagnostics_.updates, diagnostics_.polls,
                    diagnostics_.published, diagnostics_.rendered);
        CE_LOG_DEBUG(CE::enginelog, "subsystem=runtime domain={} operation=summary superseded={} skipped={} dropped_batches={} dropped_ns={} peak_polls={} resizes={}",
                     diagnostics_.domain, diagnostics_.superseded, diagnostics_.skipped_publication,
                     diagnostics_.dropped_batches, diagnostics_.dropped_nanoseconds, diagnostics_.peak_polls, diagnostics_.resizes);
        if (diagnostics_.dropped_batches)
            CE_LOG_WARN(CE::enginelog, "subsystem=runtime domain={} operation=lag_recovery outcome=dropped batches={} duration_ns={}",
                        diagnostics_.domain, diagnostics_.dropped_batches, diagnostics_.dropped_nanoseconds);
    }

    void GameRuntime::preserve_failure(std::exception_ptr& first, const char* phase, std::exception_ptr next) {
        if (next && !first)
            phase_ = phase;
        else if (next && first != next)
            Diagnostics::report_outcome("runtime", diagnostics_.domain, phase, "secondary_failure");
        Diagnostics::preserve_failure(first, phase, std::move(next));
    }

    void GameRuntime::run_sequential() {
        if (stop_requested_.load(std::memory_order_acquire)) {
            finish_unstarted_session();
            return;
        }

        auto& renderer = engine_.renderer();
        auto& input = engine_.input();
        RenderAPIs::RenderFrame frame;
        bool renderer_started = false;
        bool input_started = false;
        bool game_started = false;
        std::exception_ptr failure;

        try {
            phase_ = "window_selection";
            auto& window = engine_.window();
            renderer_started = true;
            phase_ = "renderer_initialize";
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
            phase_ = "input_initialize";
            input.initialize(window);
            CE_LOG_INFO(CE::enginelog, "subsystem=input domain={} operation=capabilities state={} events={} text={} focus={}",
                        diagnostics_.domain, input.supports(Input::InputMode::State), input.supports(Input::InputMode::Events),
                        input.supports(Input::InputMode::Text), input.supports_focus());
            game_started = true;
            phase_ = "game_init";
            game_.init();
            if (!stop_requested_.load(std::memory_order_acquire)) {
                phase_ = "platform_dispatch";
                engine_.platform_dispatcher().drain(engine_);
            }

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
                phase_ = "platform_dispatch";
                engine_.platform_dispatcher().drain(engine_);
                // Sequential execution cannot poll during update(), but spacing
                // still applies. A delayed poll never delays simulation or rendering.
                if (backlog.poll_due(Input::InputClock::now())) {
                    phase_ = "input_poll";
                    input.poll();
                    ++diagnostics_.polls;
                    window.check_native_failure();
                    if (stop_requested_.load(std::memory_order_acquire) || window.should_close())
                        break;
                    backlog.complete(input.poll_snapshot(), Input::InputClock::now());
                    diagnostics_.peak_polls = std::max<std::uint64_t>(diagnostics_.peak_polls, backlog.completed_polls());
                }
                if (stop_requested_.load(std::memory_order_acquire) || window.should_close())
                    break;
                const auto size = window.framebuffer_size();
                if (size != viewport) {
                    renderer.set_viewport(size);
                    viewport = size;
                    ++diagnostics_.resizes;
                }
                const auto batch = timing.advance(SimulationClock::now());
                if (batch.dropped > SimulationClock::duration::zero()) {
                    ++diagnostics_.dropped_batches;
                    diagnostics_.dropped_nanoseconds += std::chrono::duration_cast<std::chrono::nanoseconds>(batch.dropped).count();
                }
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
                    phase_ = "game_update";
                    ++diagnostics_.updates;
                    game_.update(TickContext{std::chrono::duration<double>(step.delta).count(), state, size, step.kind, dropped});
                    updated = true;
                    if (stop_requested_.load(std::memory_order_acquire))
                        break;
                }
                if (!stop_requested_.load(std::memory_order_acquire)) {
                    phase_ = "platform_dispatch";
                    engine_.platform_dispatcher().drain(engine_);
                }

                // Publish only the final useful state from the bounded batch.
                // Retain that complete frame for cycles without a simulation update.
                if (updated) {
                    phase_ = "prepare_frame";
                    frame.recycle();
                    RenderAPIs::RenderFrameWriter writer(frame);
                    game_.prepare_render_frame(writer);
                    published = true;
                    ++diagnostics_.published;
                }
                if (published) {
                    phase_ = "render_present";
                    renderer.clear();
                    renderer.render(frame);
                    engine_.surface().present();
                    ++diagnostics_.rendered;
                }
                phase_ = "renderer_maintenance";
                renderer.maintain_resources();
                std::unique_lock lock(scheduler_->mutex);
                const auto deadline =
                    std::min({timing.next_update_at(), backlog.next_poll_at(), SimulationClock::now() + resource_maintenance_interval});
                scheduler_->wake.wait_until(lock, deadline, [&] {
                    return stop_requested_.load(std::memory_order_acquire) || engine_.platform_dispatcher().has_pending();
                });
            }
        } catch (...) {
            failure = std::current_exception();
        }

        const auto finish = [this, &failure](const char* phase, auto&& operation) {
            try {
                operation();
            } catch (...) {
                preserve_failure(failure, phase, std::current_exception());
            }
        };
        stop();
        finish("close worker submissions", [this] { engine_.close_worker_submissions(); });
        finish("close simulation dispatcher", [this] { simulation_dispatcher_.close(); });
        if (game_started)
            finish("game quiesce", [this] { game_.quiesce(); });
        finish_worker_shutdown(failure);
        finish("pending native window callback", [this] { engine_.window().check_native_failure(); });
        finish("close platform dispatcher", [this] { engine_.platform_dispatcher().close(); });
        // Frames and game dependencies remain alive through accepted CPU work.
        finish("recycle frame", [&frame] { frame.recycle(); });
        if (game_started)
            finish("game deinit", [this] { game_.deinit(); });
        if (renderer_ready_)
            finish("renderer maintenance", [&renderer] { renderer.maintain_resources(); });
        // Adapters must also clean up if initialize() only completed partway.
        if (input_started)
            finish("input deinitialize", [&input] { input.deinitialize(); });
        if (renderer_started)
            finish("renderer deinitialize", [&renderer] { renderer.deinitialize(); });
        renderer_ready_ = false;
        if (failure)
            std::rethrow_exception(failure);
    }

    void GameRuntime::run_concurrent() {
        if (stop_requested_.load(std::memory_order_acquire)) {
            finish_unstarted_session();
            return;
        }

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
            const char* worker_phase = "simulation_start";
        };

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
            phase_ = "window_selection";
            auto& window = engine_.window();
            renderer_started = true;
            phase_ = "renderer_initialize";
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
            phase_ = "input_initialize";
            input.initialize(window);
            CE_LOG_INFO(CE::enginelog, "subsystem=input domain={} operation=capabilities state={} events={} text={} focus={}",
                        diagnostics_.domain, input.supports(Input::InputMode::State), input.supports(Input::InputMode::Events),
                        input.supports(Input::InputMode::Text), input.supports_focus());
            game_started = true;
            phase_ = "game_init";
            game_.init();
            if (!stop_requested_.load(std::memory_order_acquire)) {
                phase_ = "platform_dispatch";
                engine_.platform_dispatcher().drain(engine_);
            }

            auto previous_poll = input.action_snapshot();
            if (!previous_poll)
                throw Exceptions::failed_operation(CE_HERE, "Input adapter did not supply an initial snapshot");
            auto viewport = window.framebuffer_size();
            renderer.set_viewport(viewport);
            handoff.framebuffer_size = viewport;

            // The worker owns simulation and its clock; the calling thread alone
            // touches the window, input adapter, renderer, and presentation surface.
            auto simulate = [&, previous_poll = std::move(previous_poll)]() mutable {
                const char* worker_phase = "simulation_bind";
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
                        if (batch.dropped > SimulationClock::duration::zero()) {
                            ++diagnostics_.dropped_batches;
                            diagnostics_.dropped_nanoseconds += std::chrono::duration_cast<std::chrono::nanoseconds>(batch.dropped).count();
                        }
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
                            worker_phase = "game_update";
                            ++diagnostics_.updates;
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
                        if (!writable) {
                            ++diagnostics_.skipped_publication;
                            continue;
                        }

                        RenderAPIs::RenderFrameWriter writer(slots[*writable].frame);
                        worker_phase = "prepare_frame";
                        game_.prepare_render_frame(writer);
                        ++diagnostics_.published;
                        {
                            std::lock_guard lock(scheduler_->mutex);
                            if (handoff.ready) {
                                slots[*handoff.ready].state = SlotState::Retired;
                                ++diagnostics_.superseded;
                            }
                            slots[*writable].state = SlotState::Ready;
                            handoff.ready = *writable;
                        }
                        scheduler_->wake.notify_all();
                    }
                } catch (...) {
                    std::lock_guard lock(scheduler_->mutex);
                    handoff.worker_failure = std::current_exception();
                    handoff.worker_phase = worker_phase;
                }
                // Release unexecuted simulation captures on their owner while
                // game/resources still exist, before publishing worker_done.
                try {
                    simulation_dispatcher_.close();
                } catch (...) {
                    const auto next = std::current_exception();
                    bool secondary = false;
                    {
                        std::lock_guard lock(scheduler_->mutex);
                        if (!handoff.worker_failure) {
                            handoff.worker_failure = next;
                            handoff.worker_phase = "simulation_dispatcher_close";
                        } else
                            secondary = handoff.worker_failure != next;
                    }
                    if (secondary)
                        Diagnostics::report_failure("simulation owner dispatcher close", next);
                }
                {
                    std::lock_guard lock(scheduler_->mutex);
                    handoff.worker_done = true;
                }
                scheduler_->wake.notify_all();
            };
            phase_ = "simulation_start";
            worker = simulation_thread_factory_ ? simulation_thread_factory_(std::move(simulate)) : std::thread(std::move(simulate));
            if (!worker.joinable())
                throw Exceptions::failed_operation(CE_HERE, "Simulation thread factory returned no thread");

            std::optional<std::size_t> current_frame;
            // Full batches pause only polling. Rendering and recycling remain
            // available, and consumption wakes the platform to resume polling.
            while (!stop_requested_.load(std::memory_order_acquire) && !window.should_close()) {
                phase_ = "platform_dispatch";
                engine_.platform_dispatcher().drain(engine_);
                bool poll_due = false;
                {
                    std::lock_guard lock(scheduler_->mutex);
                    if (handoff.worker_done)
                        break;
                    poll_due = handoff.backlog.poll_due(Input::InputClock::now());
                }

                if (poll_due) {
                    phase_ = "input_poll";
                    input.poll();
                    ++diagnostics_.polls;
                    window.check_native_failure();
                    if (stop_requested_.load(std::memory_order_acquire) || window.should_close())
                        break;
                    auto completed = input.poll_snapshot();
                    if (!completed)
                        throw Exceptions::failed_operation(CE_HERE, "Input adapter did not publish a snapshot");
                    const auto size = window.framebuffer_size();
                    if (size != viewport) {
                        renderer.set_viewport(size);
                        viewport = size;
                        ++diagnostics_.resizes;
                    }
                    {
                        std::lock_guard lock(scheduler_->mutex);
                        // Every completed poll consumes capacity, including an
                        // unchanged observation. No completed observation is discarded.
                        handoff.backlog.complete(std::move(completed), Input::InputClock::now());
                        diagnostics_.peak_polls = std::max<std::uint64_t>(diagnostics_.peak_polls, handoff.backlog.completed_polls());
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
                    phase_ = "render_present";
                    renderer.clear();
                    renderer.render(slots[*current_frame].frame);
                    engine_.surface().present();
                    ++diagnostics_.rendered;
                }
                // Retirement must progress even before the first frame, while
                // input capacity is full, or while a slow update produces no frame.
                phase_ = "renderer_maintenance";
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
        const auto finish = [this, &failure](const char* phase, auto&& operation) {
            try {
                operation();
            } catch (...) {
                preserve_failure(failure, phase, std::current_exception());
            }
        };
        finish("close worker submissions", [this] { engine_.close_worker_submissions(); });
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
        std::exception_ptr worker_failure;
        const char* worker_phase;
        {
            std::lock_guard lock(scheduler_->mutex);
            worker_failure = handoff.worker_failure;
            worker_phase = handoff.worker_phase;
        }
        preserve_failure(failure, worker_phase, std::move(worker_failure));
        // Also handles initialization/thread-start failure before owner binding.
        finish("close simulation dispatcher", [this] { simulation_dispatcher_.close(); });
        if (game_started)
            finish("game quiesce", [this] { game_.quiesce(); });
        finish_worker_shutdown(failure);
        finish("pending native window callback", [this] { engine_.window().check_native_failure(); });
        finish("close platform dispatcher", [this] { engine_.platform_dispatcher().close(); });
        for (auto& slot : slots)
            finish("recycle frame", [&slot] { slot.frame.recycle(); });
        if (game_started)
            finish("game deinit", [this] { game_.deinit(); });
        if (renderer_ready_)
            finish("renderer maintenance", [&renderer] { renderer.maintain_resources(); });
        if (input_started)
            finish("input deinitialize", [&input] { input.deinitialize(); });
        if (renderer_started)
            finish("renderer deinitialize", [&renderer] { renderer.deinitialize(); });
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

    void GameRuntime::finish_unstarted_session() {
        std::exception_ptr failure;
        const auto finish = [this, &failure](const char* phase, auto&& operation) {
            try {
                operation();
            } catch (...) {
                preserve_failure(failure, phase, std::current_exception());
            }
        };
        finish("close worker submissions", [this] { engine_.close_worker_submissions(); });
        finish("close simulation dispatcher", [this] { simulation_dispatcher_.close(); });
        finish("close platform dispatcher", [this] { engine_.platform_dispatcher().close(); });
        // Neither mailbox has opened, so no accepted platform continuation can
        // require pumping; queued CPU jobs observe closed targets and settle.
        finish("finish workers", [this] { engine_.finish_workers(); });
        if (failure)
            std::rethrow_exception(failure);
    }

    void GameRuntime::pump_shutdown_requests(std::exception_ptr& failure) {
        try {
            engine_.platform_dispatcher().drain(engine_);
        } catch (...) {
            preserve_failure(failure, "shutdown platform dispatch", std::current_exception());
            // If dispatch itself fails, cancel rather than strand futures which
            // a worker is waiting for. Preserve the first failure during cleanup.
            try {
                engine_.platform_dispatcher().close();
            } catch (...) {
                preserve_failure(failure, "shutdown platform cancellation", std::current_exception());
            }
        }
        // Maintenance failure is separate from dispatcher failure: keep servicing
        // accepted CPU-to-platform completions while retaining the original error.
        if (renderer_ready_) {
            try {
                engine_.renderer().maintain_resources();
            } catch (...) {
                preserve_failure(failure, "shutdown renderer maintenance", std::current_exception());
            }
        }
    }

    void GameRuntime::finish_worker_shutdown(std::exception_ptr& failure) {
        while (!engine_.workers_idle()) {
            pump_shutdown_requests(failure);
            std::unique_lock lock(scheduler_->mutex);
            // Worker accounting has no borrowed runtime wake callback. A bounded
            // wait observes completion while platform posts wake immediately.
            scheduler_->wake.wait_for(lock, std::chrono::milliseconds{1}, [this] { return engine_.platform_dispatcher().has_pending(); });
        }
        try {
            engine_.finish_workers();
        } catch (...) {
            preserve_failure(failure, "finish workers", std::current_exception());
        }
    }
}
