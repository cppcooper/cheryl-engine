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
        virtual void init() = 0;
        virtual void quiesce() {}
        virtual void deinit() = 0;
        virtual void update(const ::CE::Game::TickContext& tick) = 0;
        virtual void prepare_render_frame(::CE::Rendering::FrameWriter& frame) const = 0;
    };
}

/**
 * @fn CE::Game::AbstractGame::~AbstractGame()
 * @brief Destroy the hook object after its runtime session and borrowed use have finished.
 * Destruction is not a replacement for runtime-controlled quiesce()/deinit().
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Game::AbstractGame::init()
 * @brief Initialize game state after renderer/input initialization on the platform owner.
 * Register bindings and upload graphics resources here. Cleanup hooks must tolerate partial initialization if this throws.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Game::AbstractGame::quiesce()
 * @brief Stop external producers after simulation stops on the platform owner.
 * Invalidate borrowed event registrations while retaining their dependencies through deinit(). Accepted CPU work has not yet settled: do not wait for jobs/callbacks requiring platform dispatch. The default is a no-op.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Game::AbstractGame::deinit()
 * @brief Release game resources and bindings on the platform/graphics owner.
 * Runtime calls this after accepted CPU work settles and frame handles are recycled, including after partial init() failure; backend adapters are still alive.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Game::AbstractGame::update(const CE::Game::TickContext& tick)
 * @brief Advance authoritative state on the selected simulation owner.
 * @param tick Borrowed context/input valid only during this invocation; simulation and observation deltas have different meanings.
 * Do not mutate platform-owned bindings or perform graphics uploads from the concurrent simulation worker. Exceptions stop the session and preserve the first failure through cleanup.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Game::AbstractGame::prepare_render_frame(CE::Rendering::FrameWriter& frame) const
 * @brief Copy render values/resources into a free frame after updates on the simulation owner.
 * @param frame Borrowed writer valid for this call; retain neither it nor derived pass writers.
 * No backend graphics calls belong here. Concurrent execution can skip preparation when no slot is free; simulation still advances.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */
