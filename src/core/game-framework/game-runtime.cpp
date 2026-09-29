#include <core/game-framework/game-runtime.h>

#include <core/game-framework/abstract-game.h>
#include <core/engine/engine-context.h>
#include <core/controls/input-interface.h>
#include <core/display/window-interface.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <internals/exceptions.h>
#include <templates/delta.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace CE::GFramework {
    GameRuntime::GameRuntime(Engine::EngineContext& engine, AbstractGame& game, const RunMode mode) :
        engine_(engine), game_(game), mode_(mode) {}

    void GameRuntime::run() {
        switch (mode_) {
        case RunMode::Sequential:
            return run_sequential();
        case RunMode::Concurrent:
            return run_concurrent();
        }
        throw Exceptions::invalid_args(CE_HERE, "Unknown game runtime mode");
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
        bool game_ready = false;
        std::exception_ptr failure;

        try {
            renderer_started = true;
            renderer.initialize();
            input_started = true;
            input.initialize(window);
            game_.init();
            game_ready = true;

            // Game initialization may register bindings and upload assets. Start
            // timing and sample the baseline only after it has completed.
            auto previous_poll = input.action_snapshot();
            if (!previous_poll)
                throw Exceptions::failed_operation(CE_HERE, "Input adapter did not supply an initial snapshot");
            auto viewport = window.framebuffer_size();
            renderer.set_viewport(viewport);
            DeltaTime clock;

            while (!stop_requested_.load(std::memory_order_acquire) && !window.should_close()) {
                // Sequential mode consumes exactly the one poll it performed for
                // this tick. The snapshot stays stable through game update.
                input.poll();
                if (stop_requested_.load(std::memory_order_acquire) || window.should_close())
                    break;
                auto completed_poll = input.action_snapshot();
                Input::TickInput tick_input(previous_poll, {completed_poll});
                const auto size = window.framebuffer_size();
                if (size != viewport) {
                    renderer.set_viewport(size);
                    viewport = size;
                }
                game_.update(TickContext{clock(), tick_input, size});
                previous_poll = std::move(completed_poll);

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
        const auto finish = [&failure](auto&& operation) {
            try { operation(); }
            catch (...) { if (!failure) failure = std::current_exception(); }
        };
        if (game_ready) finish([this] { game_.deinit(); });
        // Adapters must also clean up if initialize() only completed partway.
        if (input_started) finish([&input] { input.deinitialize(); });
        if (renderer_started) finish([&renderer] { renderer.deinitialize(); });
        if (failure) std::rethrow_exception(failure);
    }

    void GameRuntime::run_concurrent() {
        if (stop_requested_.load(std::memory_order_acquire))
            return;

        enum class SlotState { Free, Writing, Ready, Rendering, Retired, Recycling };
        struct Slot {
            RenderAPIs::RenderFrame frame;
            SlotState state = SlotState::Free; // Guarded by scheduler_mutex_.
        };
        struct Handoff {
            std::vector<std::shared_ptr<const Input::ActionSnapshot>> polls;
            FramebufferSize framebuffer_size;
            std::optional<std::size_t> ready;
            std::exception_ptr worker_failure;
            bool worker_done = false;
        };

        auto& window = engine_.window();
        auto& renderer = engine_.renderer();
        auto& input = engine_.input();
        std::array<Slot, 3> slots;
        Handoff handoff;
        constexpr auto cadence = std::chrono::microseconds{16667};
        std::thread worker;
        bool renderer_started = false;
        bool input_started = false;
        bool game_ready = false;
        std::exception_ptr failure;

        try {
            renderer_started = true;
            renderer.initialize();
            input_started = true;
            input.initialize(window);
            game_.init();
            game_ready = true;

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
                    DeltaTime clock;
                    auto next_tick = std::chrono::steady_clock::now() + cadence;
                    while (!stop_requested_.load(std::memory_order_acquire)) {
                        std::vector<std::shared_ptr<const Input::ActionSnapshot>> polls;
                        FramebufferSize size;
                        {
                            std::unique_lock lock(scheduler_mutex_);
                            scheduler_wake_.wait_until(lock, next_tick, [&] {
                                return stop_requested_.load(std::memory_order_acquire);
                            });
                            if (stop_requested_.load(std::memory_order_acquire))
                                break;
                            polls.swap(handoff.polls);
                            size = handoff.framebuffer_size;
                        }
                        // A slow tick never triggers a burst of catch-up updates.
                        // If rendering blocks polling, held input persists without replaying edges.
                        next_tick = std::chrono::steady_clock::now() + cadence;

                        Input::TickInput tick_input(previous_poll, std::move(polls));
                        game_.update(TickContext{clock(), tick_input, size});
                        previous_poll = tick_input.latest_poll();
                        if (stop_requested_.load(std::memory_order_acquire))
                            break;

                        // Never wait for the renderer to release a slot. Simulation
                        // keeps updating; only a visual snapshot is skipped.
                        std::optional<std::size_t> writable;
                        {
                            std::lock_guard lock(scheduler_mutex_);
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
                            std::lock_guard lock(scheduler_mutex_);
                            if (handoff.ready)
                                slots[*handoff.ready].state = SlotState::Retired;
                            slots[*writable].state = SlotState::Ready;
                            handoff.ready = *writable;
                        }
                        scheduler_wake_.notify_all();
                    }
                } catch (...) {
                    std::lock_guard lock(scheduler_mutex_);
                    handoff.worker_failure = std::current_exception();
                }
                {
                    std::lock_guard lock(scheduler_mutex_);
                    handoff.worker_done = true;
                }
                scheduler_wake_.notify_all();
            });

            // With no completed frame, keep pumping events at a bounded rate.
            // Rendering can wake this loop sooner; a long present may delay a poll.
            auto next_poll = std::chrono::steady_clock::now();
            while (!stop_requested_.load(std::memory_order_acquire) && !window.should_close()) {
                {
                    std::lock_guard lock(scheduler_mutex_);
                    if (handoff.worker_done)
                        break;
                }

                if (std::chrono::steady_clock::now() >= next_poll) {
                    input.poll();
                    if (stop_requested_.load(std::memory_order_acquire) || window.should_close())
                        break;
                    auto completed = input.action_snapshot();
                    if (!completed)
                        throw Exceptions::failed_operation(CE_HERE, "Input adapter did not publish a snapshot");
                    const auto size = window.framebuffer_size();
                    if (size != viewport) {
                        renderer.set_viewport(size);
                        viewport = size;
                    }
                    {
                        std::lock_guard lock(scheduler_mutex_);
                        handoff.polls.push_back(std::move(completed));
                        handoff.framebuffer_size = size;
                    }
                    next_poll = std::chrono::steady_clock::now() + cadence;
                }

                // Superseded frames still hold asset handles. Only this thread
                // may recycle them, while the graphics context is current.
                for (auto& slot : slots) {
                    {
                        std::lock_guard lock(scheduler_mutex_);
                        if (slot.state != SlotState::Retired)
                            continue;
                        slot.state = SlotState::Recycling;
                    }
                    slot.frame.recycle();
                    {
                        std::lock_guard lock(scheduler_mutex_);
                        slot.state = SlotState::Free;
                    }
                }

                std::optional<std::size_t> ready;
                {
                    std::lock_guard lock(scheduler_mutex_);
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
                        std::lock_guard lock(scheduler_mutex_);
                        slots[*ready].state = SlotState::Free;
                    }
                }

                std::unique_lock lock(scheduler_mutex_);
                scheduler_wake_.wait_until(lock, next_poll, [&] {
                    if (stop_requested_.load(std::memory_order_acquire) || handoff.worker_done || handoff.ready)
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
        if (worker.joinable()) worker.join();
        // The worker cannot be writing now. Even incomplete frames must release
        // their handles before deinitializing the game or graphics context.
        const auto finish = [&failure](auto&& operation) {
            try { operation(); }
            catch (...) { if (!failure) failure = std::current_exception(); }
        };
        for (auto& slot : slots) finish([&slot] { slot.frame.recycle(); });
        {
            std::lock_guard lock(scheduler_mutex_);
            if (!failure) failure = handoff.worker_failure;
        }
        if (game_ready) finish([this] { game_.deinit(); });
        if (input_started) finish([&input] { input.deinitialize(); });
        if (renderer_started) finish([&renderer] { renderer.deinitialize(); });
        if (failure) std::rethrow_exception(failure);
    }

    void GameRuntime::stop() {
        {
            std::lock_guard lock(scheduler_mutex_);
            stop_requested_.store(true, std::memory_order_release);
        }
        scheduler_wake_.notify_all();
    }
}
