#pragma once

#include <cheryl/export.h>

#include <core/controls/tick-input.h>
#include <core/display/framebuffer-size.h>
#include <core/display/viewport.h>

#include "simulation-scheduler.h"

#include <stop_token>

namespace CE::Game {
    /** One simulation update. The runtime owns the input view for the duration of update().
     * delta_seconds belongs to the simulation policy; input.elapsed() and raw
     * hold/down durations remain observation time. A recovery batch reports its
     * dropped time once, on the final update. Input never subdivides updates.
     * framebuffer_size and logical_size are sampled together on the platform thread
     * and copied into this tick. Manually authored ticks default logical_size to zero.
     */
    struct TickContext {
        double delta_seconds; ///< Simulation-policy delta in seconds; distinct from observed_seconds().
        const ::CE::Input::Tick& input; ///< Borrowed accumulated input view, valid during the game update.
        ::CE::Platform::FramebufferSize framebuffer_size; ///< Copied drawable pixel extent sampled with logical_size on the platform owner.
        ::CE::Runtime::Simulation::UpdateKind update_kind = ::CE::Runtime::Simulation::UpdateKind::Variable; ///< Selected ordinary or recovery update kind.
        double dropped_seconds = 0.0; ///< Lag discarded by policy, reported once at the recovery batch's final update.
        ::CE::Platform::Dimensions<int> logical_size{0, 0}; ///< Copied logical window size; manually authored ticks default to zero.
        std::stop_source runtime_stop{std::nostopstate}; ///< Shared runtime stop state, or unavailable for a manually authored tick.

        CE_API void request_stop() const;
        [[nodiscard]] double observed_seconds() const { return input.elapsed().count(); }
        // Derived control policy: observed down-time proportion times simulation delta.
        // With no observation interval, use held State. This does not recover tap history.
        [[nodiscard]] CE_API double button_simulation_seconds(::CE::Input::Actions::Id action) const;
    };
}

/**
 * @fn void CE::Game::TickContext::request_stop() const
 * @brief Request normal runtime shutdown without interrupting the current update.
 * @throws CE::Exceptions::failed_operation If this manually authored tick has no stop state.
 * Return explicitly when the game should stop processing this tick; normal teardown occurs afterward.
 * @see [Detailed guide](cheryl-guide:runtime/simulation-timing)
 */

/**
 * @fn double CE::Game::TickContext::observed_seconds() const
 * @brief Read accumulated input observation time in seconds.
 * @return input.elapsed().count(), independent of the selected simulation delta.
 * @see [Detailed guide](cheryl-guide:runtime/simulation-timing)
 */

/**
 * @fn double CE::Game::TickContext::button_simulation_seconds(CE::Input::Actions::Id action) const
 * @brief Scale simulation delta by an observed direct action's down-time fraction.
 * @param action Direct/global action ID queried through this tick's input view.
 * @return Clamped observed down-time proportion times delta_seconds; when no observation interval exists, uses current held state.
 * @throws CE::Exceptions::invalid_args If delta_seconds is negative or nonfinite.
 * This derived control policy does not reconstruct missing tap history or replace per-context action lookup.
 * @see [Detailed guide](cheryl-guide:runtime/simulation-timing)
 */
