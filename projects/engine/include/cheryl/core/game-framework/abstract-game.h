#pragma once

#include <cheryl/export.h>

#include <core/rendering/render-frame.h>
#include "tick-context.h"

namespace CE::Game {
    /** Game simulation hooks driven by GameRuntime.
     * update() receives accumulated input and the selected simulation delta, with
     * input observation time available separately, regardless of whether
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
     * Hook failures stop the session; runtime cleanup preserves the first failure.
     */
    struct CE_API AbstractGame {
        virtual ~AbstractGame() = default;
        /** Initialize game state after rendering and input adapters are ready.
         * Register bindings and upload resources on the platform/graphics owner.
         * Cleanup hooks must tolerate this call failing after partial initialization.
         */
        virtual void init() = 0;
        /** Stop external producers after simulation stops on the platform owner.
         * Invalidate borrowed event registrations, but retain dependencies until
         * deinit(): accepted CPU work has not yet settled. Do not wait here for
         * callbacks/jobs requiring platform dispatch. The default implementation is empty.
         */
        virtual void quiesce() {}
        /** Release game resources and bindings on the platform/graphics owner.
         * Called after accepted CPU work settles and frame handles are recycled,
         * including when init() failed partway; backend adapters remain alive.
         */
        virtual void deinit() = 0;
        /** Advance authoritative game state on the selected simulation owner.
         * @param tick Borrowed update context and input view valid for this call;
         * its delta belongs to simulation policy, not raw input observation time.
         */
        virtual void update(const ::CE::Game::TickContext& tick) = 0;
        /** Copy render state into a free frame after simulation updates.
         * This runs on the simulation owner and can be skipped when no slot is free.
         * @param frame Borrowed writer valid only during this preparation; retain
         * neither it nor pass writers, and perform no backend graphics operations.
         */
        virtual void prepare_render_frame(::CE::Rendering::FrameWriter& frame) const = 0;
    };
}
