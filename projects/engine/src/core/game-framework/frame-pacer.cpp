#include <core/game-framework/frame-pacer.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace CE::Runtime {
    FramePacer::FramePacer(FramePacingOptions options)
    : options_(options) {
        if (!std::isfinite(options.target_fps) || options.target_fps <= 0)
            throw CE::Exceptions::invalid_args(CE_HERE, "Target FPS must be finite and positive");
        const auto ticks =
            std::ceil(static_cast<long double>(Clock::period::den) / (static_cast<long double>(Clock::period::num) * options.target_fps));
        // A strict bound avoids an out-of-range cast when long double has the
        // same precision as double and rounds the representation's max upward.
        if (!std::isfinite(ticks) || ticks >= static_cast<long double>(std::numeric_limits<Clock::rep>::max()))
            throw CE::Exceptions::invalid_args(CE_HERE, "Target FPS period exceeds the monotonic clock duration range");
        interval_ = Clock::duration{static_cast<Clock::rep>(std::max(1.0L, ticks))};
    }

    bool FramePacer::due(Clock::time_point now) const noexcept {
        return !options_.enabled || !next_ || now >= *next_;
    }

    FramePacer::Clock::time_point FramePacer::next_frame_at() const noexcept {
        return options_.enabled && next_ ? *next_ : Clock::time_point::max();
    }

    void FramePacer::frame_started(Clock::time_point now) noexcept {
        if (options_.enabled)
            next_ = now > Clock::time_point::max() - interval_ ? Clock::time_point::max() : now + interval_;
    }
}
