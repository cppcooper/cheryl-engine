#pragma once

#include <core/controls/tick-input.h>
#include <core/display/framebuffer-size.h>

namespace CE::GFramework {
    /** One simulation update. The runtime owns the input view for the duration of update().
     * delta_seconds advances the interval in which input has this state. Several
     * updates may run before a single render frame when polls accumulated.
     * framebuffer_size is sampled on the platform thread and copied into this tick.
     */
    struct TickContext {
        double delta_seconds;
        const Input::TickInput& input;
        FramebufferSize framebuffer_size;
    };
}
