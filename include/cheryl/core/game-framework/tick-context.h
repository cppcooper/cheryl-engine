#pragma once

#include <core/controls/tick-input.h>

namespace CE::GFramework {
    /** One simulation update. The runtime owns the input view for the duration of update().
     * delta_seconds comes from the simulation clock, independently of platform polling.
     */
    struct TickContext {
        double delta_seconds;
        const Input::TickInput& input;
    };
}
