#include "climate-checks.h"

#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace ProcGen;
    ClimateContext example_context() {
        ClimateContext c;
        c.expected_revision = {1, 1};
        c.frame = {2000, 2000, 10, 0, 40, 0, RowNorth::decreasing};
        c.calendar = {86'400, 360, 3600, 0, 23.5};
        return c;
    }

    void report(const char* label, ClimateInput input, const ClimateBaseline& baseline) {
        ProcGenClimateChecks::inspect(input, baseline);
        const auto& d = baseline.descriptor();
        const auto m = baseline.measurements();
        std::cout << label << ": regional_solar_baseline model" << d.context.model_version << "/numerics" << d.context.numerics_version
                  << "; tick=" << d.tick << "; model_s/tick=" << d.context.calendar.seconds_per_tick
                  << "; source revision=" << d.revision.terrain << '/' << d.revision.surface << "; x-fast=" << d.domain.extents()[0] << 'x'
                  << d.domain.extents()[1] << "; rows=north-negative"
                  << "; dx/dy_m=" << d.context.frame.dx_metres << '/' << d.context.frame.dy_metres
                  << "; tick_m=" << d.context.frame.tick_metres << "; datum_m=" << d.context.frame.datum_metres
                  << "; centre_lat/lon_deg=" << d.context.frame.centre_latitude_degrees << '/' << d.context.frame.centre_longitude_degrees
                  << '\n';
        std::cout << "  z_m=" << m.minimum_z_metres << ".." << m.maximum_z_metres << "; seasonal_air_target_K=" << m.minimum_air_kelvin
                  << ".." << m.maximum_air_kelvin << "; mean_instant_absorbed_W/m2=" << m.mean_absorbed_w_m2
                  << "; mean_daily_cosine=" << m.mean_daily_cosine << "; sunlit_cells=" << m.sunlit_cells
                  << "; finite/replenished_water_cell_equivalents=" << m.finite_water_cell_equivalents << '/'
                  << m.replenished_water_cell_equivalents << "; work=" << baseline.usage().work
                  << "; payload_bytes=" << baseline.usage().storage_bytes << '\n';
        const auto& point = baseline.cells()[baseline.cells().size() / 2];
        std::cout << "  centre sun_ENU=" << point.sun.east << ',' << point.sun.north << ',' << point.sun.up << "; reference_at_"
                  << d.context.model.reference_metres << "m_K=" << point.seasonal_reference_kelvin
                  << "; elevation_corrected_air_K=" << point.air_reference_kelvin
                  << "; instant_response_target_K=" << point.instant_target_kelvin
                  << "; retained_thermal_response_s=" << point.response_seconds << '\n';
    }
}
int main() {
    unsigned passed = 0;
    try {
        std::cout << std::fixed << std::setprecision(4);
        const auto d = Domain::checked(std::array<Index, 2>{3, 1}).value();
        const std::array<Height, 3> heights{0, 100, 0};
        const std::array<ClimateSurface, 3> coast{
            {{0, 1, .1, .01, 21'600, 432'000}, {0, 0, .25, .8, 21'600, 432'000}, {1, 0, .1, .01, 21'600, 432'000}}
        };
        ClimateContext context = example_context();
        context.frame = {1000, 1000, 10, 0, 0, 0, RowNorth::decreasing};
        context.calendar.tilt_degrees = 0;
        const ClimateInput literal{d, heights, coast, {1, 1}};
        for (Index tick : {12U, 0U}) {
            const auto baseline = derive_climate(literal, context, tick, climate_hard_limits).value();
            report(tick == 12 ? "Literal ridge/coast noon" : "Literal ridge/coast night", literal, baseline);
            if (baseline.cells()[1].air_reference_kelvin >= baseline.cells()[1].seasonal_reference_kelvin)
                throw std::logic_error("ridge lapse direction");
            ++passed;
        }
        const auto e =
            generate_elevation(std::array<Index, 2>{17, 11}, {42, 0x67656f6772617068, 7}, {3, -80, 120}, elevation_hard_limits).value();
        std::vector<ClimateSurface> surface(187);
        // Explicit application coast: western4 columns replenished, a finite pool
        // at centre. No sea-threshold, spill-depth or U11-count inference.
        for (std::size_t i = 0; i < 187; ++i) {
            if (i % 17 < 4)
                surface[i] = {0, 1, .1, .02, 21'600, 432'000};
            else if (i == 93)
                surface[i] = {1, 0, .15, .03, 21'600, 432'000};
        }
        context = example_context();
        context.expected_revision = {8, 12};
        const ClimateInput generated{e.descriptor().domain, e.heights(), surface, {8, 12}};
        auto summer = derive_climate(e, surface, {8, 12}, context, 90 * 24 + 12, climate_hard_limits).value();
        auto winter = derive_climate(e, surface, {8, 12}, context, 270 * 24 + 12, climate_hard_limits).value();
        auto night = derive_climate(e, surface, {8, 12}, context, 270 * 24, climate_hard_limits).value();
        if (!summer.descriptor().elevation || summer.descriptor().elevation.value() != e.descriptor())
            throw std::logic_error("lost exact U8 identity");
        std::cout << "Owned U8 lattice_bilinear v1 key{42,0x67656f6772617068,7}; pitch3/-80..120 abstract ticks; caller chooses10m/tick.\n";
        report("Northern summer noon", generated, summer);
        ++passed;
        report("Northern winter noon", generated, winter);
        ++passed;
        report("Northern winter night", generated, night);
        ++passed;
        if (!(summer.measurements().mean_daily_cosine > winter.measurements().mean_daily_cosine &&
              winter.measurements().mean_absorbed_w_m2 > night.measurements().mean_absorbed_w_m2 && night.measurements().sunlit_cells == 0))
            throw std::logic_error("missing seasonal/diurnal consequences");
        std::cout
            << "Game reference targets only; no evolving weather, ocean inference, evaporation, measured climate normal or rainfall.\n"
            << "Climate consumer: " << passed << " passed, 0 failed, 0 skipped.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Climate consumer failed after " << passed << " demonstrations: " << e.what() << '\n';
        return 1;
    }
}
