#pragma once

#include "tick-context.h"
#include <core/rendering/render-frame.h>

namespace CE::GFramework {
    /** Game simulation hooks driven by GameRuntime.
     * update() receives input assembled for one simulation tick, regardless of whether
     * simulation runs on the platform thread or on a worker. The game is free to organize
     * its logic without a prescribed controller or state-machine architecture.
     * Drawing does not run on this interface. After update(), the runtime lends a free
     * frame slot to prepare_render_frame() on the simulation thread, then publishes it.
     * The thread and service requirements of init()/deinit() also remain to be defined.
     */
    struct AbstractGame {
        virtual ~AbstractGame() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void update(const TickContext& tick) = 0;
        virtual void prepare_render_frame(RenderAPIs::RenderFrameWriter& frame) const = 0;
    };
}
