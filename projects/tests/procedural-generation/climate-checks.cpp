#include "climate-checks.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ProcGenClimateChecks {
    namespace {
        void require(bool v) {
            if (!v)
                throw std::runtime_error("independent climate source/field check failed");
        }
        void near(double a, double b) {
            require(std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)}));
        }
    }
    void inspect(ProcGen::ClimateInput in, const ProcGen::ClimateBaseline& out) {
        require(in.domain.cells() <= 16'384 && in.domain == out.descriptor().domain && in.revision == out.descriptor().revision);
        require(in.heights.size() == out.cells().size() && in.surface.size() == out.cells().size());
        const auto& c = out.descriptor().context;
        const auto w = static_cast<std::size_t>(in.domain.extents()[0]), h = static_cast<std::size_t>(in.domain.extents()[1]);
        double zmin = 12'000, zmax = -12'000, tmin = 1000, tmax = 0, absorbed = 0, mean = 0, finite = 0, replenished = 0;
        ProcGen::Index lit = 0;
        for (std::size_t i = 0; i < out.cells().size(); ++i) {
            const auto& a = out.cells()[i];
            const auto& s = in.surface[i];
            require(a.height_tick == in.heights[i] && a.surface == s);
            near(a.z_metres, static_cast<double>(in.heights[i]) * c.frame.tick_metres + c.frame.datum_metres);
            const auto x = i % w, y = i / w;
            const auto xi = [&](std::size_t xx, std::size_t yy) { return in.heights[xx + yy * w]; };
            const auto left = x == 0 ? x : x - 1, right = x + 1 == w ? x : x + 1;
            const auto top = y == 0 ? y : y - 1, bottom = y + 1 == h ? y : y + 1;
            const double dx = static_cast<double>(right - left) * c.frame.dx_metres;
            const double dy = static_cast<double>(bottom - top) * c.frame.dy_metres;
            near(
                a.east_slope,
                dx == 0 ? 0 : static_cast<double>(static_cast<std::int64_t>(xi(right, y)) - xi(left, y)) * c.frame.tick_metres / dx
            );
            near(
                a.north_slope, dy == 0 ? 0
                                       : static_cast<double>(static_cast<std::int64_t>(xi(x, bottom)) - xi(x, top)) * c.frame.tick_metres /
                                             dy * (c.frame.north == ProcGen::RowNorth::increasing ? 1 : -1)
            );
            const auto v = a.sun;
            near(v.east * v.east + v.north * v.north + v.up * v.up, 1);
            require(v.daily_mean_cosine >= 0 && v.daily_mean_cosine <= 1 && v.daylight_fraction >= 0 && v.daylight_fraction <= 1);
            const double slope_norm = std::hypot(1.0, std::hypot(a.east_slope, a.north_slope));
            const double intercept =
                v.up <= 0 ? 0 : std::clamp((v.up - a.east_slope * v.east - a.north_slope * v.north) / slope_norm, 0.0, 1.0);
            near(a.surface_cosine, intercept);
            near(a.absorbed_w_m2, intercept * c.model.solar_w_m2 * c.model.transmission * (1 - s.albedo));
            near(
                a.seasonal_reference_kelvin, c.model.cooling_floor_kelvin + v.daily_mean_cosine * c.model.solar_w_m2 *
                                                                                c.model.transmission * (1 - s.albedo) /
                                                                                c.model.cooling_w_m2_kelvin
            );
            near(
                a.seasonal_reference_kelvin - a.air_reference_kelvin,
                (a.z_metres - c.model.reference_metres) * c.model.lapse_kelvin_per_metre
            );
            near(
                a.instant_target_kelvin, c.model.cooling_floor_kelvin + a.absorbed_w_m2 / c.model.cooling_w_m2_kelvin -
                                             c.model.lapse_kelvin_per_metre * (a.z_metres - c.model.reference_metres)
            );
            near(
                a.response_seconds, s.land_response_seconds + (s.finite_water_fraction + s.replenished_water_fraction) *
                                                                  (s.water_response_seconds - s.land_response_seconds)
            );
            zmin = std::min(zmin, a.z_metres);
            zmax = std::max(zmax, a.z_metres);
            tmin = std::min(tmin, a.air_reference_kelvin);
            tmax = std::max(tmax, a.air_reference_kelvin);
            absorbed += a.absorbed_w_m2;
            mean += v.daily_mean_cosine;
            finite += s.finite_water_fraction;
            replenished += s.replenished_water_fraction;
            if (intercept > 0)
                ++lit;
        }
        const auto m = out.measurements();
        near(m.minimum_z_metres, zmin);
        near(m.maximum_z_metres, zmax);
        near(m.minimum_air_kelvin, tmin);
        near(m.maximum_air_kelvin, tmax);
        near(m.mean_absorbed_w_m2, absorbed / static_cast<double>(in.heights.size()));
        near(m.mean_daily_cosine, mean / static_cast<double>(in.heights.size()));
        near(m.finite_water_cell_equivalents, finite);
        near(m.replenished_water_cell_equivalents, replenished);
        require(m.sunlit_cells == lit);
    }
}
