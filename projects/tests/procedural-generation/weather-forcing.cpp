#include "weather-kernel.h"
#include <cmath>
#include <numbers>

namespace ProcGen::WeatherDetail {
    WeatherResult<void> forcing(
        const ClimateBaseline& b,
        const WeatherModel& m,
        Index tick,
        std::span<double> potential,
        std::span<double> east,
        std::span<double> north,
        Work& work
    ) {
        const auto& d = b.descriptor().domain;
        const auto w = d.extents()[0], h = d.extents()[1];
        const auto& frame = b.descriptor().context.frame;
        const auto& config = m.forcing;
        constexpr double pi = std::numbers::pi;
        const double sign = frame.north == RowNorth::increasing ? 1 : -1;
        std::array<std::array<double, 2>, 4> phase{};
        for (Index k = 0; k < config.count; ++k) {
            auto rng = IntegerStream::for_stage({derive_stream_state(config.semantic), curl_stage, k});
            for (auto& angle : phase[static_cast<std::size_t>(k)]) {
                if (!work.consume(1))
                    return std::unexpected(WeatherFailure{FailureKind::budget_refused, WeatherReason::work});
                angle = static_cast<double>(rng.next() >> 11) * (2 * pi / 9007199254740992.0);
            }
        }
        for (Index y = 0; y <= h; ++y)
            for (Index x = 0; x <= w; ++x) {
                if (!work.consume(1 + 2 * config.count))
                    return std::unexpected(WeatherFailure{FailureKind::budget_refused, WeatherReason::work});
                auto& psi = potential[static_cast<std::size_t>(x + y * (w + 1))];
                if (m.boundary == MoistureBoundary::periodic && y == h) {
                    psi = potential[static_cast<std::size_t>(x)];
                    continue;
                }
                if (m.boundary == MoistureBoundary::periodic && x == w) {
                    psi = potential[static_cast<std::size_t>(y * (w + 1))];
                    continue;
                }
                psi = 0;
                if (m.boundary == MoistureBoundary::closed && (x == 0 || x == w || y == 0 || y == h))
                    continue;
                const double e = config.origin_east_metres + (static_cast<double>(x) - static_cast<double>(w) / 2) * frame.dx_metres;
                const double n =
                    config.origin_north_metres + sign * (static_cast<double>(y) - static_cast<double>(h) / 2) * frame.dy_metres;
                for (Index k = 0; k < config.count; ++k) {
                    const auto& scale = config.scales[static_cast<std::size_t>(k)];
                    const double time = 2 * pi * static_cast<double>(tick % scale.period_ticks) / static_cast<double>(scale.period_ticks);
                    psi += scale.potential_m2_s2 *
                           std::sin(
                               2 * pi * e / (static_cast<double>(scale.wavelength_cells) * frame.dx_metres) +
                               phase[static_cast<std::size_t>(k)][0] + time
                           ) *
                           std::sin(
                               2 * pi * n / (static_cast<double>(scale.wavelength_cells) * frame.dy_metres) +
                               phase[static_cast<std::size_t>(k)][1] - time
                           );
                }
                if (m.boundary == MoistureBoundary::closed) {
                    const double sx = std::sin(pi * static_cast<double>(x) / static_cast<double>(w)),
                                 sy = std::sin(pi * static_cast<double>(y) / static_cast<double>(h));
                    psi *= sx * sx * sy * sy;
                }
            }
        for (Index y = 0; y < h; ++y)
            for (Index x = 0; x <= w; ++x) {
                if (!work.consume(1))
                    return std::unexpected(WeatherFailure{FailureKind::budget_refused, WeatherReason::work});
                east[static_cast<std::size_t>(x + y * (w + 1))] =
                    sign *
                    (potential[static_cast<std::size_t>(x + (y + 1) * (w + 1))] - potential[static_cast<std::size_t>(x + y * (w + 1))]) /
                    frame.dy_metres;
            }
        for (Index y = 0; y <= h; ++y)
            for (Index x = 0; x < w; ++x) {
                if (!work.consume(1))
                    return std::unexpected(WeatherFailure{FailureKind::budget_refused, WeatherReason::work});
                north[static_cast<std::size_t>(x + y * w)] =
                    -(potential[static_cast<std::size_t>(x + 1 + y * (w + 1))] - potential[static_cast<std::size_t>(x + y * (w + 1))]) /
                    frame.dx_metres;
            }
        return {};
    }
}
