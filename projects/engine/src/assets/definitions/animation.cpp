#include <assets/definitions/animation.h>
#include <internals/exceptions.h>

#include <limits>

namespace CE::Assets {
    CellIndex TileAnimationDefinition::cell_at(const std::chrono::milliseconds elapsed) const {
        if (elapsed.count() < 0)
            throw Exceptions::invalid_args(CE_HERE, "Tile animation elapsed time must be nonnegative");
        if (frames.empty())
            throw Exceptions::invalid_args(CE_HERE, "Tile animation needs at least one frame");

        std::chrono::milliseconds total{};
        for (const auto& frame : frames) {
            if (frame.duration.count() <= 0)
                throw Exceptions::invalid_args(CE_HERE, "Tile animation durations must be positive");
            if (frame.duration.count() > std::numeric_limits<std::chrono::milliseconds::rep>::max() - total.count())
                throw Exceptions::invalid_args(CE_HERE, "Tile animation duration exceeds the millisecond range");
            total += frame.duration;
        }

        auto offset = elapsed.count();
        if (loop)
            offset %= total.count();
        for (const auto& frame : frames) {
            if (offset < frame.duration.count())
                return frame.cell;
            offset -= frame.duration.count();
        }
        return frames.back().cell;
    }
}
