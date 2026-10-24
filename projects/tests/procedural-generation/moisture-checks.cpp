#include "moisture-checks.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ProcGenMoistureChecks {
    namespace {
        void require(bool b) {
            if (!b)
                throw std::runtime_error("independent moisture ledger/state check failed");
        }
        void near(double a, double b) {
            require(std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)}));
        }
    }
    ProcGen::ClimateContext context(double dx, double dy, ProcGen::Index seconds) {
        ProcGen::ClimateContext c;
        c.expected_revision = {1, 1};
        c.frame = {dx, dy, 10, 0, 40, 0, ProcGen::RowNorth::decreasing};
        c.calendar = {86'400, 360, seconds, 0, 23.5};
        return c;
    }
    ProcGen::ClimateBaseline baseline(
        ProcGen::Index w,
        ProcGen::Index h,
        std::span<const ProcGen::Height> z,
        std::span<const ProcGen::ClimateSurface> surface,
        const ProcGen::ClimateContext& c,
        ProcGen::Index tick
    ) {
        const auto d = ProcGen::Domain::checked(std::array<ProcGen::Index, 2>{w, h}).value();
        return ProcGen::derive_climate({d, z, surface, c.expected_revision}, c, tick, ProcGen::climate_hard_limits).value();
    }
    ProcGen::MoistureModel passive_model() {
        ProcGen::MoistureModel m;
        m.condensation_per_second = 0;
        m.cloud_evaporation_per_second = 0;
        m.rain_per_second = 0;
        m.surface_evaporation_mm_per_second = 0;
        return m;
    }
    Winds uniform_winds(
        const ProcGen::Domain& d,
        const ProcGen::RegionalFrame& frame,
        double east,
        double north,
        ProcGen::MoistureBoundary boundary
    ) {
        static_cast<void>(frame); // physical east/north arrays, production maps row sign
        const auto w = d.extents()[0], h = d.extents()[1];
        Winds wind{
            std::vector<double>(static_cast<std::size_t>((w + 1) * h), east),
            std::vector<double>(static_cast<std::size_t>(w * (h + 1)), north), boundary
        };
        if (boundary == ProcGen::MoistureBoundary::closed) {
            for (ProcGen::Index y = 0; y < h; ++y)
                wind.east[static_cast<std::size_t>(y * (w + 1))] = wind.east[static_cast<std::size_t>(w + y * (w + 1))] = 0;
            for (ProcGen::Index x = 0; x < w; ++x)
                wind.north[static_cast<std::size_t>(x)] = wind.north[static_cast<std::size_t>(x + h * w)] = 0;
        }
        return wind;
    }
    void inspect(const ProcGen::MoistureState& old, const ProcGen::MoistureAdvance& after) {
        const auto& state = after.state;
        const auto& r = after.report;
        const auto& l = r.water;
        require(old.cells().size() == state.cells().size() && r.transfers.size() == state.cells().size());
        require(r.from_tick == old.tick() && r.to_tick == state.tick() && r.to_tick > r.from_tick);
        require(r.seconds_per_tick == old.baseline().descriptor().context.calendar.seconds_per_tick);
        require(state.baseline().descriptor() == old.baseline().descriptor());
        require(std::ranges::equal(state.baseline().cells(), old.baseline().cells()) && state.model() == old.model());
        const double area = old.baseline().descriptor().context.frame.dx_metres * old.baseline().descriptor().context.frame.dy_metres;
        double before = 0, final = 0, finite = 0, source = 0, condensed = 0, evaporated = 0, rain = 0;
        for (std::size_t i = 0; i < state.cells().size(); ++i) {
            const auto a = old.cells()[i], b = state.cells()[i];
            const auto t = r.transfers[i];
            require(b.vapour_mm >= 0 && b.cloud_mm >= 0 && b.finite_surface_mm >= 0 && std::isfinite(b.reference_kelvin));
            require(
                t.finite_evaporation_mm >= 0 && t.replenished_mm >= 0 && t.condensed_mm >= 0 && t.cloud_evaporated_mm >= 0 && t.rain_mm >= 0
            );
            near(a.finite_surface_mm - b.finite_surface_mm, t.finite_evaporation_mm);
            before += a.vapour_mm + a.cloud_mm + a.finite_surface_mm;
            final += b.vapour_mm + b.cloud_mm + b.finite_surface_mm;
            finite += t.finite_evaporation_mm;
            source += t.replenished_mm;
            condensed += t.condensed_mm;
            evaporated += t.cloud_evaporated_mm;
            rain += t.rain_mm;
        }
        near(l.initial_m3, before * area * .001);
        near(l.final_m3, final * area * .001);
        near(l.finite_evaporation_m3, finite * area * .001);
        near(l.replenished_m3, source * area * .001);
        near(l.condensed_m3, condensed * area * .001);
        near(l.cloud_evaporated_m3, evaporated * area * .001);
        near(l.rain_export_m3, rain * area * .001);
        require(l.boundary_in_m3 == 0 && l.boundary_out_m3 == 0);
        near(l.final_m3 + l.rain_export_m3, l.initial_m3 + l.replenished_m3);
        require(std::abs(l.residual_m3) <= l.tolerance_m3);
        near(l.residual_m3, l.final_m3 + l.rain_export_m3 - l.initial_m3 - l.replenished_m3);
    }
}
