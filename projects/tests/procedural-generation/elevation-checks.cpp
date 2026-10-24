#include "elevation-checks.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ProcGenElevationChecks {
    void inspect(const ProcGen::Elevation& field) {
        using namespace ProcGen;
        const auto require = [](bool ok) {
            if (!ok) {
                throw std::runtime_error("Independent signed elevation oracle mismatch");
            }
        };
        const auto& replay = field.descriptor();
        const auto& d = replay.domain;
        require(d.dimensions() == 2 && d.cells() <= 4096 && d.cells() == field.heights().size());
        const auto c = replay.requested;
        require(c.spacing >= 1 && c.spacing <= 4096 && c.low <= c.high && replay.version == 1);
        const auto w = d.extents()[0], h = d.extents()[1], s = c.spacing;
        const auto nx = (w - 1) / s + static_cast<Index>((w - 1) % s != 0) + 1;
        const auto ny = (h - 1) / s + static_cast<Index>((h - 1) % s != 0) + 1;
        auto stream = IntegerStream::for_stage({derive_stream_state(replay.random), elevation_lattice_stage, 0});
        const auto bound = static_cast<Index>(static_cast<std::int64_t>(c.high) - c.low) + 1;
        const auto threshold = (Index{0} - bound) % bound;
        std::vector<std::int64_t> anchors;
        for (Index i = 0; i < nx * ny; ++i) {
            Index raw = 0;
            unsigned attempts = 0;
            do {
                require(++attempts <= 1024);
                raw = stream.next();
            } while (raw < threshold);
            anchors.push_back(c.low + static_cast<std::int64_t>(raw % bound));
        }
        Height minimum = std::numeric_limits<Height>::max(), maximum = std::numeric_limits<Height>::min();
        for (Index y = 0; y < h; ++y) {
            for (Index x = 0; x < w; ++x) {
                const auto ix = x / s, iy = y / s;
                std::int64_t numerator = 0;
                for (Index dy = 0; dy < 2; ++dy) {
                    for (Index dx = 0; dx < 2; ++dx) {
                        const auto weight_x = dx == 0 ? s - x % s : x % s;
                        const auto weight_y = dy == 0 ? s - y % s : y % s;
                        numerator += anchors[static_cast<std::size_t>(std::min(iy + dy, ny - 1) * nx + std::min(ix + dx, nx - 1))] *
                                     static_cast<std::int64_t>(weight_x) * static_cast<std::int64_t>(weight_y);
                    }
                }
                const auto denominator = static_cast<std::int64_t>(s * s);
                auto expected = numerator / denominator;
                if (numerator < 0 && numerator % denominator != 0) {
                    --expected;
                }
                const auto index = y * w + x;
                const auto value = field.height(index).value();
                require(value == expected && value >= c.low && value <= c.high);
                require(d.index(std::array<Index, 2>{x, y}).value() == index);
                const auto coordinate = d.coordinates(index).value();
                require(coordinate[0] == x && coordinate[1] == y);
                minimum = std::min(minimum, value);
                maximum = std::max(maximum, value);
            }
        }
        require(
            (field.measurements() ==
             ElevationMeasurements{minimum, maximum, static_cast<Index>(static_cast<std::int64_t>(maximum) - minimum)})
        );
    }
}
