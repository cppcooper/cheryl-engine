#pragma once

#include <core/controls/tick-input.h>
#include <core/display/framebuffer-size.h>

#include "simulation-scheduler.h"

namespace CE::GFramework {
    /** One simulation update. The runtime owns the input view for the duration of update().
     * delta_seconds belongs to the simulation policy; input.elapsed() and raw
     * hold/down durations remain observation time. A recovery batch reports its
     * dropped time once, on the final update. Input never subdivides updates.
     * framebuffer_size is sampled on the platform thread and copied into this tick.
     */
    struct TickContext {
        double delta_seconds;
        const Input::TickInput& input;
        FramebufferSize framebuffer_size;
        UpdateKind update_kind = UpdateKind::Variable;
        double dropped_seconds = 0.0;

        [[nodiscard]] double observed_seconds() const { return input.elapsed().count(); }
        // Derived control policy: observed down-time proportion times simulation delta.
        // With no observation interval, use held State. This does not recover tap history.
        [[nodiscard]] double button_simulation_seconds(
            Input::ActionId action
        ) const;
    };
}
