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
     * init_platform() and deinit_platform() run on the platform/graphics thread, before
     * the worker starts and after it joins. init_simulation(), update(), prepare_render_frame()
     * and deinit_simulation() run on the simulation owner. In concurrent mode this is the
     * worker; sequential mode uses the caller for both owners. The game must not change
     * input bindings or upload GPU resources from the worker.
     * Hook failures stop the session; runtime cleanup preserves the first failure.
     */
    struct CE_API AbstractGame {
        virtual ~AbstractGame() = default;
        virtual void init_platform() {}
        virtual void init_simulation() {}
        virtual void deinit_simulation() {}
        virtual void quiesce() {}
        virtual void deinit_platform() {}
        virtual void update(const ::CE::Game::TickContext& tick) = 0;
        virtual void prepare_render_frame(::CE::Rendering::FrameWriter& frame) const = 0;
    };
}

/**
 * @fn CE::Game::AbstractGame::~AbstractGame()
 * @brief Destroy the hook object after its runtime session and borrowed use have finished.
 * Destruction is not a replacement for runtime-controlled cleanup hooks.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Game::AbstractGame::init_platform()
 * @brief Initialize platform state after renderer/input initialization, before simulation starts.
 * Register bindings and upload graphics resources here. The default is a no-op. deinit_platform()
 * runs even if this hook throws and must tolerate partial initialization. Use the constructor
 * for setup without runtime/thread dependencies; there is no generic runtime init() hook.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Game::AbstractGame::init_simulation()
 * @brief Initialize simulation state on its owner after services and the initial view are available.
 * Runs once after init_platform(), before Ready, simulation timing and the first update. Attach
 * simulation services and view watchers here so they participate from the first step. The default
 * is a no-op. A stop request before entry skips this hook. Failure prevents Ready/updates and
 * still calls deinit_simulation() on this owner for partial cleanup.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Game::AbstractGame::deinit_simulation()
 * @brief Release state requiring the simulation owner after its dispatcher closes and before it exits.
 * Paired with attempted init_simulation(), including partial failure. The default is a no-op.
 * Producers and accepted CPU work can still be active: retain their dependencies until quiesce()
 * and deinit_platform(). No further simulation dispatch runs. Platform dispatch remains serviced
 * in concurrent mode; never block sequential execution on work requiring its own platform thread.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Game::AbstractGame::quiesce()
 * @brief Stop external producers after simulation stops on the platform owner.
 * Invalidate borrowed event registrations while retaining their dependencies through deinit_platform(). Accepted CPU work has not yet settled: do not wait for jobs/callbacks requiring platform dispatch. The default is a no-op.
 * @see [Detailed guide](cheryl-guide:runtime/runtime-frame-boundary)
 */

/**
 * @fn void CE::Game::AbstractGame::deinit_platform()
 * @brief Release game resources and bindings on the platform/graphics owner.
 * Runtime calls this after accepted CPU work settles and frame handles are recycled, including
 * after partial init_platform() failure; backend adapters are still alive. The default is a no-op.
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
