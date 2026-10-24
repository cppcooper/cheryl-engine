#include "moisture-checks.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace ProcGen;
    namespace C = ProcGenMoistureChecks;
    void report(const char* label, const MoistureState& old, const MoistureAdvance& result) {
        C::inspect(old, result);
        const auto& l = result.report.water;
        const auto& s = result.state;
        const auto w = s.baseline().descriptor().domain.extents()[0];
        double mass = 0, moment = 0;
        for (std::size_t i = 0; i < s.cells().size(); ++i) {
            mass += s.cells()[i].vapour_mm;
            moment += s.cells()[i].vapour_mm * static_cast<double>(i % w);
        }
        std::cout << label << ": face_upwind_moisture v" << s.model().version << "/numerics" << s.model().numerics_version
                  << "; ticks=" << result.report.from_tick << ".." << result.report.to_tick
                  << "; model_s/tick=" << result.report.seconds_per_tick << "; substeps/tick=" << s.usage().substeps_per_tick
                  << "; vapour_centre_x_cells=" << (mass == 0 ? 0 : moment / mass) << "; work=" << s.usage().work
                  << "; payload_bytes=" << s.usage().storage_bytes << '\n';
        std::cout << "  water_m3: initial=" << l.initial_m3 << "; final=" << l.final_m3 << "; finite_to_vapour=" << l.finite_evaporation_m3
                  << "; external_replenished=" << l.replenished_m3 << "; condensed=" << l.condensed_m3
                  << "; cloud_re_evap=" << l.cloud_evaporated_m3 << "; exported_rain=" << l.rain_export_m3
                  << "; boundary_in/out=" << l.boundary_in_m3 << '/' << l.boundary_out_m3 << "; residual=" << l.residual_m3
                  << "; tolerance=" << l.tolerance_m3 << '\n';
        std::cout << "  rain_mm by row:";
        for (std::size_t y = 0; y < s.cells().size() / static_cast<std::size_t>(w); ++y) {
            double sum = 0;
            for (std::size_t x = 0; x < w; ++x)
                sum += result.report.transfers[x + y * static_cast<std::size_t>(w)].rain_mm;
            std::cout << ' ' << sum;
        }
        std::cout << "\n  uniform dx*dy area; reference air proxy gets ONE local lapse correction; rain exported, not U11 discharge.\n";
    }
    MoistureAdvance ridge(unsigned kind, bool mirror = false) {
        const Index w = 7, h = 3;
        std::vector<Height> heights(21);
        std::vector<ClimateSurface> surface(21);
        for (std::size_t y = 0; y < 3; ++y) {
            if (kind != 0 && !(kind == 2 && y == 1))
                heights[3 + y * 7] = 100;
            surface[(mirror ? 6 : 0) + y * 7].replenished_water_fraction = 1; // explicit coast, evaporation disabled in this pulse proof
        }
        const auto c = C::context(1000, 1000, 1000);
        const auto b = C::baseline(w, h, heights, surface, c);
        std::vector<MoistureCell> initial(21);
        for (std::size_t y = 0; y < 3; ++y)
            initial[(mirror ? 5 : 1) + y * 7].vapour_mm = 9;
        auto model = C::passive_model();
        model.condensation_per_second = 1;
        model.rain_per_second = 1;
        auto old = start_moisture(b, initial, model, moisture_hard_limits).value();
        auto winds = C::uniform_winds(b.descriptor().domain, c.frame, mirror ? -1 : 1, 0, MoistureBoundary::closed);
        auto next = advance_moisture(old, b, winds.view(), 4, moisture_hard_limits).value();
        report(
            kind == 0   ? "Flat wet pulse"
            : kind == 2 ? "Ridge with central gap"
            : mirror    ? "Mirrored wind AND wet source"
                        : "Full ridge",
            old, next
        );
        return next;
    }
}
int main() {
    unsigned passed = 0;
    try {
        std::cout << std::fixed << std::setprecision(8);
        const auto flat = ridge(0);
        ++passed;
        const auto full = ridge(1);
        ++passed;
        const auto gap = ridge(2);
        ++passed;
        const auto mirrored = ridge(1, true);
        ++passed;
        if (!(flat.report.water.rain_export_m3 == 0 && full.report.water.rain_export_m3 > gap.report.water.rain_export_m3 &&
              gap.report.water.rain_export_m3 > 0 &&
              std::abs(full.report.water.rain_export_m3 - mirrored.report.water.rain_export_m3) < 1e-9))
            throw std::logic_error("missing causal ridge/gap/mirror rainout");
        const auto e =
            generate_elevation(std::array<Index, 2>{17, 11}, {42, 0x67656f6772617068, 7}, {3, -80, 120}, elevation_hard_limits).value();
        auto context = C::context(2000, 2000, 1000);
        std::vector<ClimateSurface> surface(187);
        std::vector<MoistureCell> initial(187);
        for (std::size_t i = 0; i < 187; ++i) {
            if (i % 17 == 0)
                surface[i].replenished_water_fraction = 1;
            if (i % 17 == 1)
                initial[i].vapour_mm = 9;
        }
        surface[93].finite_water_fraction = 1;
        initial[93].finite_surface_mm = .02;
        const auto baseline = derive_climate(e, surface, {1, 1}, context, 0, climate_hard_limits).value();
        auto model = C::passive_model();
        model.condensation_per_second = 1;
        model.rain_per_second = 1;
        model.surface_evaporation_mm_per_second = .00001;
        const auto old = start_moisture(baseline, initial, model, moisture_hard_limits).value();
        auto winds = C::uniform_winds(baseline.descriptor().domain, context.frame, 2, 0, MoistureBoundary::closed);
        const auto next = advance_moisture(old, baseline, winds.view(), 8, moisture_hard_limits).value();
        std::cout
            << "Actual retained U8 lattice_bilinear v1 key{42,0x67656f6772617068,7},17x11/pitch3/-80..120; explicit10m/tick/datum0, closed2m/s east flow; finite pool0.02mm and replenished western coast.\n";
        report("Generated mountain wet pulse", old, next);
        ++passed;
        if (!(next.report.water.rain_export_m3 > 0 && next.report.water.finite_evaporation_m3 > 0 && next.report.water.replenished_m3 > 0))
            throw std::logic_error("missing generated rain/supply/depletion");
        std::cout
            << "Capacity is a game column proxy, not RH/dew point. No latent heat, pressure solver, ocean inference or physical forecast.\nMoisture consumer: "
            << passed << " passed, 0 failed, 0 skipped.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Moisture consumer failed after " << passed << ": " << e.what() << '\n';
        return 1;
    }
}
