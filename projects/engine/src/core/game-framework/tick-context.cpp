#include <core/game-framework/tick-context.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>

namespace CE::GFramework {
    void TickContext::request_stop() const {
        if (!runtime_stop.stop_possible())
            throw Exceptions::failed_operation(CE_HERE, "Tick has no runtime stop request");
        (void)runtime_stop.request_stop();
    }

    double TickContext::button_simulation_seconds(const Input::ActionId action) const {
        if (!std::isfinite(delta_seconds) || delta_seconds < 0.0)
            throw Exceptions::invalid_args(CE_HERE, "Input simulation contribution requires a finite nonnegative delta");
        const auto state = input.button(action);
        const auto observed = observed_seconds();
        const auto fraction = observed > 0.0 ? std::clamp(state.down_duration.count() / observed, 0.0, 1.0) : state.held() ? 1.0 : 0.0;
        return fraction * delta_seconds;
    }
}
