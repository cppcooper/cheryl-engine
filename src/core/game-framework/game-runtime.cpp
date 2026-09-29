#include <core/game-framework/game-runtime.h>

#include <core/game-framework/abstract-game.h>
#include <core/engine/engine-context.h>
#include <core/controls/input-interface.h>
#include <core/display/window-interface.h>
#include <core/rendering/presentation-surface.h>
#include <core/rendering/renderer.h>
#include <internals/exceptions.h>
#include <templates/delta.h>

#include <exception>
#include <utility>

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
        // TODO: After platform-thread initialization, start one worker for update and frame preparation.
        // Keep input polling, rendering, presentation, and frame recycling on the calling thread;
        // join the worker and release game/GPU resources before leaving the graphics context.
        // A stop request must also wake any worker waiting for input or a free frame slot.
        throw Exceptions::failed_operation(CE_HERE, "Concurrent game runtime is a skeleton");
    }

    void GameRuntime::stop() { stop_requested_.store(true, std::memory_order_release); }
}
