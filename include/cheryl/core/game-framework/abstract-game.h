#pragma once

#include <core/rendering/render-frame.h>
#include "tick-context.h"

namespace CE::GFramework {
    /** Game simulation hooks driven by GameRuntime.
     * update() receives one input state and its elapsed time, regardless of whether
     * simulation runs on the platform thread or on a worker. The game is free to organize
     * its logic without a prescribed controller or state-machine architecture.
     * Drawing does not run on this interface. After advancing simulation,
     * the runtime may lend a free frame slot to prepare_render_frame() on the
     * simulation thread, then publish it.
     * Concurrent mode can skip preparation when all slots are occupied; later updates
     * still advance the authoritative simulation state.
     * init() and deinit() run on the platform/graphics thread, before the worker starts and
     * after it joins. In concurrent mode, update() and prepare_render_frame() run only on that
     * worker. The game must not change input bindings or upload GPU resources from the worker.
     */
    struct AbstractGame {
        virtual ~AbstractGame() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void update(const TickContext& tick) = 0;
        virtual void prepare_render_frame(RenderAPIs::RenderFrameWriter& frame) const = 0;
    };
}
