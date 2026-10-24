#include "climate.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

namespace ProcGen {
    namespace {
        constexpr double pi = std::numbers::pi, rad = pi / 180, radius = 6'371'000;
        struct Stop {
            ClimateFailure failure;
        };
        [[noreturn]] void fail(FailureKind kind, ClimateReason reason) {
            throw Stop{{kind, reason}};
        }
        void charge(Work& work, Index units) {
            if (!work.consume(units))
                fail(FailureKind::budget_refused, ClimateReason::work);
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b)
                fail(FailureKind::invalid_input, ClimateReason::overflow);
            return a + b;
        }
        Index mul(Index a, Index b) {
            if (b != 0 && a > std::numeric_limits<Index>::max() / b)
                fail(FailureKind::invalid_input, ClimateReason::overflow);
            return a * b;
        }
        bool within(double v, double low, double high) {
            return std::isfinite(v) && v >= low && v <= high;
        }
        void calendar_ok(const SolarCalendar& c) {
            if (c.day_seconds < 60 || c.day_seconds > 1'000'000 || c.year_days < 4 || c.year_days > 4096 || c.seconds_per_tick == 0 ||
                c.seconds_per_tick > 1'000'000 || !within(c.tilt_degrees, 0, 60))
                fail(FailureKind::invalid_input, ClimateReason::calendar);
        }
        SunSample solar(const SolarCalendar& c, Index tick, double latitude, double longitude) {
            const Index period = c.day_seconds * c.year_days; // validated<=4096000000
            const auto signed_period = static_cast<std::int64_t>(period);
            const auto remainder = c.epoch_seconds % signed_period;
            const auto epoch = static_cast<Index>(remainder < 0 ? remainder + signed_period : remainder);
            const Index phase = (epoch + (tick % period) * c.seconds_per_tick % period) % period;
            const double declination =
                std::asin(std::sin(c.tilt_degrees * rad) * std::sin(2 * pi * (static_cast<double>(phase) / static_cast<double>(period))));
            const double hour =
                2 * pi * (static_cast<double>(phase % c.day_seconds) / static_cast<double>(c.day_seconds) - 0.5) + longitude * rad;
            const double sl = std::sin(latitude * rad), cl = std::abs(latitude) == 90 ? 0 : std::cos(latitude * rad);
            const double sd = std::sin(declination), cd = std::cos(declination);
            const double a = sl * sd, b = cl * cd;
            double up = a + b * std::cos(hour);
            if (std::abs(up) <= 1e-14)
                up = 0;
            double mean = 0, daylight = 0;
            if (b == 0) {
                mean = a > 1e-14 ? a : 0;
                daylight = a > 1e-14 ? 1 : 0;
            } else if (a >= b) {
                mean = std::max(0.0, a);
                daylight = 1;
            } else if (a > -b) {
                const double h = std::acos(std::clamp(-a / b, -1.0, 1.0));
                mean = (a * h + b * std::sin(h)) / pi;
                daylight = h / pi;
            }
            return {-cd * std::sin(hour), cl * sd - sl * cd * std::cos(hour), up, std::max(0.0, mean), daylight};
        }
        ClimateRequirements needed(const Domain& d) {
            if (d.dimensions() != 2)
                fail(FailureKind::invalid_input, ClimateReason::dimensions);
            const auto bytes = add(climate_fixed_storage, mul(d.cells(), sizeof(ClimateCell)));
            const auto work = add(64, mul(32, d.cells()));
            if (d.cells() > std::vector<ClimateCell>{}.max_size())
                fail(FailureKind::invalid_input, ClimateReason::overflow);
            return {d.cells(), bytes, work};
        }
        void context_ok(const ClimateContext& c, ClimateRevision revision, const Domain& d) {
            if (c.model_version != 1 || c.numerics_version != 1 || c.surface_version != 1)
                fail(FailureKind::unsupported, ClimateReason::version);
            if (revision.terrain == 0 || revision.surface == 0 || revision != c.expected_revision)
                fail(FailureKind::invalid_input, ClimateReason::revision);
            const auto& f = c.frame;
            if (!within(f.dx_metres, 1, 10'000) || !within(f.dy_metres, 1, 10'000) || !std::isfinite(f.tick_metres) || f.tick_metres <= 0 ||
                f.tick_metres > 1000 || !within(f.datum_metres, -12'000, 12'000) || !within(f.centre_latitude_degrees, -90, 90) ||
                !within(f.centre_longitude_degrees, -180, 180) || (f.north != RowNorth::increasing && f.north != RowNorth::decreasing))
                fail(FailureKind::invalid_input, ClimateReason::frame);
            if (std::abs(f.centre_latitude_degrees) > 60 || static_cast<double>(d.extents()[0]) * f.dx_metres > 256'000 ||
                static_cast<double>(d.extents()[1]) * f.dy_metres > 256'000)
                fail(FailureKind::unsupported, ClimateReason::projection);
            calendar_ok(c.calendar);
            const auto& m = c.model;
            if (!within(m.solar_w_m2, 0, 2000) || !within(m.transmission, 0, 1) || !within(m.cooling_floor_kelvin, 250, 350) ||
                !within(m.cooling_w_m2_kelvin, 10, 100) || !within(m.lapse_kelvin_per_metre, 0, 0.01) ||
                !within(m.reference_metres, -12'000, 12'000))
                fail(FailureKind::invalid_input, ClimateReason::model);
        }
        void surface_ok(const ClimateSurface& s) {
            if (!within(s.finite_water_fraction, 0, 1) || !within(s.replenished_water_fraction, 0, 1) ||
                s.finite_water_fraction + s.replenished_water_fraction > 1 || !within(s.albedo, 0, 1) ||
                !within(s.roughness_metres, 0, 100) || !within(s.land_response_seconds, 1, 100'000'000) ||
                !within(s.water_response_seconds, 1, 100'000'000))
                fail(FailureKind::invalid_input, ClimateReason::surface);
        }
        struct Physical {
            double z, east_slope, north_slope, latitude, longitude;
        };
        Physical physical(ClimateInput in, const RegionalFrame& f, Index i) {
            const Index w = in.domain.extents()[0], h = in.domain.extents()[1], x = i % w, y = i / w;
            const auto z = [&](Index cell) {
                return f.datum_metres + f.tick_metres * static_cast<double>(in.heights[static_cast<std::size_t>(cell)]);
            };
            const auto slope = [&](Index a, Index b, double spacing) { return (z(b) - z(a)) / spacing; };
            const double east_slope =
                w == 1 ? 0 : slope(x == 0 ? i : i - 1, x + 1 == w ? i : i + 1, f.dx_metres * (x == 0 || x + 1 == w ? 1 : 2));
            const double row_slope =
                h == 1 ? 0 : slope(y == 0 ? i : i - w, y + 1 == h ? i : i + w, f.dy_metres * (y == 0 || y + 1 == h ? 1 : 2));
            const double sign = f.north == RowNorth::increasing ? 1 : -1;
            const double east = (static_cast<double>(x) - static_cast<double>(w - 1) / 2) * f.dx_metres;
            const double north = sign * (static_cast<double>(y) - static_cast<double>(h - 1) / 2) * f.dy_metres;
            return {
                z(i), east_slope, sign * row_slope, f.centre_latitude_degrees + north / radius / rad,
                f.centre_longitude_degrees + east / (radius * std::cos(f.centre_latitude_degrees * rad)) / rad
            };
        }
        void physical_ok(const Physical& p) {
            if (!within(p.z, -12'000, 12'000) || !within(p.east_slope, -10, 10) || !within(p.north_slope, -10, 10))
                fail(FailureKind::unsupported, ClimateReason::heights);
            if (!within(p.latitude, -65, 65) || !within(p.longitude, -180, 180))
                fail(FailureKind::unsupported, ClimateReason::projection);
        }
        static_assert(sizeof(ClimateCell) == 184 && sizeof(Height) == 4 && sizeof(double) == 8);
        static_assert(2 * sizeof(ClimateBaseline) + 2 * sizeof(ClimateDescriptor) + 2048 <= climate_fixed_storage);
    }
    std::string_view climate_reason_text(ClimateReason reason) {
        switch (reason) {
            case ClimateReason::dimensions:
                return "climate requires positive checked2D domain";
            case ClimateReason::shape:
                return "climate source arrays must exactly match domain";
            case ClimateReason::revision:
                return "climate input revisions must be nonzero and match expected pair";
            case ClimateReason::frame:
                return "invalid metric/orientation/geographic context";
            case ClimateReason::projection:
                return "unsupported regional footprint/latitude/dateline";
            case ClimateReason::heights:
                return "unsupported transformed heights/slopes";
            case ClimateReason::surface:
                return "invalid explicit surface fractions/response";
            case ClimateReason::calendar:
                return "invalid solar calendar";
            case ClimateReason::model:
                return "invalid baseline model parameters";
            case ClimateReason::overflow:
                return "climate index/storage/work capacity overflow";
            case ClimateReason::cells:
                return "climate cell/axis cap refused";
            case ClimateReason::storage:
                return "climate accounted storage refused";
            case ClimateReason::work:
                return "climate work refused or exhausted";
            case ClimateReason::version:
                return "unsupported climate model/numerics/surface version";
        }
        return "unknown climate failure";
    }
    ClimateResult<SunSample> sample_sun(const SolarCalendar& c, Index tick, double latitude, double longitude) {
        try {
            calendar_ok(c);
            if (!within(latitude, -90, 90) || !within(longitude, -180, 180))
                fail(FailureKind::invalid_input, ClimateReason::frame);
            return solar(c, tick, latitude, longitude);
        } catch (const Stop& s) {
            return std::unexpected(s.failure);
        }
    }
    ClimateResult<ClimateRequirements> climate_requirements(const Domain& d) {
        try {
            return needed(d);
        } catch (const Stop& s) {
            return std::unexpected(s.failure);
        }
    }
    ClimateBaseline::ClimateBaseline(ClimateDescriptor d, std::vector<ClimateCell> cells, ClimateMeasurements m, ClimateUsage u)
    : descriptor_(std::move(d)), cells_(std::move(cells)), measured_(m), usage_(u) {}
    ClimateResult<ClimateBaseline> derive_climate(ClimateInput input, const ClimateContext& c, Index tick, CaveBudgets budgets) {
        Work work(std::min(budgets.work, climate_hard_limits.work));
        try {
            const auto req = needed(input.domain);
            if (input.heights.size() != req.cells || input.surface.size() != req.cells)
                fail(FailureKind::invalid_input, ClimateReason::shape);
            if (req.cells > std::min(budgets.cells, climate_hard_limits.cells) || input.domain.extents()[0] > 128 ||
                input.domain.extents()[1] > 128)
                fail(FailureKind::budget_refused, ClimateReason::cells);
            if (req.storage_bytes > std::min(budgets.storage_bytes, climate_hard_limits.storage_bytes))
                fail(FailureKind::budget_refused, ClimateReason::storage);
            if (req.work > std::min(budgets.work, climate_hard_limits.work))
                fail(FailureKind::budget_refused, ClimateReason::work);
            charge(work, 64);
            context_ok(c, input.revision, input.domain);
            for (Index i = 0; i < req.cells; ++i) {
                charge(work, 8);
                surface_ok(input.surface[static_cast<std::size_t>(i)]);
                physical_ok(physical(input, c.frame, i));
            }
            charge(work, req.cells); // account output value-initialization before allocation
            std::vector<ClimateCell> cells(static_cast<std::size_t>(req.cells));
            ClimateMeasurements m{12'000, -12'000, 1000, 0, 0, 0, 0, 0, 0};
            for (Index i = 0; i < req.cells; ++i) {
                charge(work, 23);
                const auto p = physical(input, c.frame, i);
                const auto s = input.surface[static_cast<std::size_t>(i)];
                const auto sun = solar(c.calendar, tick, p.latitude, p.longitude);
                const double norm = std::sqrt(1 + p.east_slope * p.east_slope + p.north_slope * p.north_slope);
                const double cosine =
                    sun.up <= 0 ? 0 : std::clamp((sun.up - p.east_slope * sun.east - p.north_slope * sun.north) / norm, 0.0, 1.0);
                const double gain = c.model.solar_w_m2 * c.model.transmission * (1 - s.albedo);
                const double seasonal = c.model.cooling_floor_kelvin + gain * sun.daily_mean_cosine / c.model.cooling_w_m2_kelvin;
                const double lapse = c.model.lapse_kelvin_per_metre * (p.z - c.model.reference_metres);
                const double water = s.finite_water_fraction + s.replenished_water_fraction;
                cells[static_cast<std::size_t>(i)] = {
                    input.heights[static_cast<std::size_t>(i)],
                    s,
                    p.z,
                    p.east_slope,
                    p.north_slope,
                    p.latitude,
                    p.longitude,
                    sun,
                    cosine,
                    gain * cosine,
                    seasonal,
                    seasonal - lapse,
                    c.model.cooling_floor_kelvin + gain * cosine / c.model.cooling_w_m2_kelvin - lapse,
                    (1 - water) * s.land_response_seconds + water * s.water_response_seconds
                };
                const auto& cell = cells[static_cast<std::size_t>(i)];
                m.minimum_z_metres = std::min(m.minimum_z_metres, p.z);
                m.maximum_z_metres = std::max(m.maximum_z_metres, p.z);
                m.minimum_air_kelvin = std::min(m.minimum_air_kelvin, cell.air_reference_kelvin);
                m.maximum_air_kelvin = std::max(m.maximum_air_kelvin, cell.air_reference_kelvin);
                m.mean_absorbed_w_m2 += cell.absorbed_w_m2;
                m.mean_daily_cosine += sun.daily_mean_cosine;
                m.finite_water_cell_equivalents += s.finite_water_fraction;
                m.replenished_water_cell_equivalents += s.replenished_water_fraction;
                if (cosine > 0)
                    ++m.sunlit_cells;
            }
            m.mean_absorbed_w_m2 /= static_cast<double>(req.cells);
            m.mean_daily_cosine /= static_cast<double>(req.cells);
            return ClimateBaseline(
                {input.domain, input.revision, c, tick, budgets, std::nullopt}, std::move(cells), m, {work.used(), req.storage_bytes}
            );
        } catch (const Stop& s) {
            auto failure = s.failure;
            failure.work = work.used();
            return std::unexpected(failure);
        }
    }
    ClimateResult<ClimateBaseline> derive_climate(
        const Elevation& e,
        std::span<const ClimateSurface> surface,
        ClimateRevision revision,
        const ClimateContext& c,
        Index tick,
        CaveBudgets budgets
    ) {
        auto result = derive_climate({e.descriptor().domain, e.heights(), surface, revision}, c, tick, budgets);
        if (result)
            result->descriptor_.elevation = e.descriptor();
        return result;
    }
}
