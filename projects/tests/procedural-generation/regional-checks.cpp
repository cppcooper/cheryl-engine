#include "regional-checks.h"
#include "moisture-checks.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ProcGenRegionalChecks {
    using namespace ProcGen;
    WeatherModel passive() {
        WeatherModel m;
        m.moisture = ProcGenMoistureChecks::passive_model();
        m.boundary = MoistureBoundary::periodic;
        m.wave_m_s = 0;
        m.rotation_per_second = 0;
        m.drag_per_second = 0;
        m.roughness_drag_per_metre_second = 0;
        m.background_east_m_s = 0;
        m.terrain_acceleration_m_s2 = 0;
        m.pressure_relaxation_per_second = 0;
        m.thermal_potential_per_kelvin = 0;
        m.air_relaxation_per_second = 0;
        m.thermal_enabled = false;
        return m;
    }
    ClimateBaseline baseline(Index w, Index h, Index seconds) {
        return ProcGenMoistureChecks::baseline(
            w, h, std::vector<Height>(static_cast<std::size_t>(w * h)), std::vector<ClimateSurface>(static_cast<std::size_t>(w * h)),
            ProcGenMoistureChecks::context(100, 100, seconds)
        );
    }
    RegionalState literal() {
        const auto b = baseline();
        auto m = passive();
        m.moisture.rain_per_second = .5;
        std::array<MoistureCell, 1> water{};
        water[0].cloud_mm = 4;
        const auto w = start_weather(b, water, std::array<FlowCell, 1>{}, m, weather_hard_limits).value();
        return start_regional(w, std::array<SoilCell, 1>{{{.25, 1, 1, .1}}}, regional_hard_limits).value();
    }
    RegionalState generated() {
        const auto e =
            generate_elevation(std::array<Index, 2>{8, 6}, {42, 0x67656f6772617068, 7}, {3, -80, 120}, elevation_hard_limits).value();
        auto c = ProcGenMoistureChecks::context(2000, 2000, 600);
        std::vector<ClimateSurface> surface(48);
        std::vector<MoistureCell> water(48);
        std::vector<FlowCell> flow(48);
        for (std::size_t i = 0; i < 48; ++i) {
            surface[i].land_response_seconds = 3600;
            surface[i].water_response_seconds = 14400;
            surface[i].roughness_metres = i % 8 == 0 ? .01 : .8;
            if (i % 8 == 0)
                surface[i].replenished_water_fraction = 1;
            if (i % 8 == 1)
                water[i].cloud_mm = 1;
            flow[i].east_m_s = 1;
        }
        surface[27].finite_water_fraction = 1;
        water[27].finite_surface_mm = .02;
        const auto b = derive_climate(e, surface, {1, 1}, c, 36, climate_hard_limits).value();
        for (std::size_t i = 0; i < 48; ++i)
            flow[i].surface_kelvin = 280 - c.model.lapse_kelvin_per_metre * b.cells()[i].z_metres;
        WeatherModel m;
        m.moisture.minimum_capacity_mm = 100;
        m.moisture.maximum_capacity_mm = 100;
        m.moisture.cloud_evaporation_per_second = 0;
        m.forcing.semantic = {42, 0x77696e64, 7};
        m.forcing.count = 2;
        m.forcing.scales[0] = {4, 8, .25};
        m.forcing.scales[1] = {8, 16, .1};
        const auto weather = start_weather(b, water, flow, m, weather_hard_limits).value();
        std::vector<SoilCell> soil(48);
        for (auto& s : soil) {
            s.capacity_mm = 2;
            s.evaporation_mm_per_second = .000001;
        }
        return start_regional(weather, soil, regional_hard_limits).value();
    }
    std::size_t inspect(const RegionalState& old, const RegionalAdvance& out) {
        std::size_t count = 0;
        auto check = [&](bool b) {
            ++count;
            if (!b)
                throw std::runtime_error("independent regional balance/transfer check failed");
        };
        auto near = [&](double a, double b) { check(std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)})); };
        double before = 0, after = 0, rain = 0, infiltration = 0, overflow = 0, evap = 0, atmos_before = 0, atmos_after = 0;
        const auto& frame = old.weather().baseline().descriptor().context.frame;
        const double unit = frame.dx_metres * frame.dy_metres / 1000;
        check(out.report.from_tick == old.weather().tick());
        check(out.report.to_tick == out.state.weather().tick());
        for (std::size_t i = 0; i < old.soil().size(); ++i) {
            const auto a = old.weather().water()[i], b = out.state.weather().water()[i];
            const auto s = out.state.soil()[i];
            const auto t = out.report.transfers[i];
            check(s.water_mm >= 0 && s.water_mm <= s.capacity_mm);
            check(b.vapour_mm >= 0 && b.cloud_mm >= 0 && b.finite_surface_mm >= 0);
            check(t.precipitation_mm >= 0 && t.infiltration_mm >= 0 && t.overflow_mm >= 0 && t.evaporation_mm >= 0);
            near(t.precipitation_mm, t.infiltration_mm + t.overflow_mm);
            near(s.water_mm, old.soil()[i].water_mm + t.infiltration_mm - t.evaporation_mm);
            const double wa = a.vapour_mm + a.cloud_mm + a.finite_surface_mm, wb = b.vapour_mm + b.cloud_mm + b.finite_surface_mm;
            atmos_before += wa;
            atmos_after += wb;
            before += wa + old.soil()[i].water_mm;
            after += wb + s.water_mm;
            rain += t.precipitation_mm;
            infiltration += t.infiltration_mm;
            overflow += t.overflow_mm;
            evap += t.evaporation_mm;
        }
        const auto& l = out.report.water;
        near(l.initial_m3, before * unit);
        near(l.final_m3, after * unit);
        near(l.precipitation_m3, rain * unit);
        near(l.infiltration_m3, infiltration * unit);
        near(l.overflow_export_m3, overflow * unit);
        near(l.soil_evaporation_m3, evap * unit);
        near((after + overflow) * unit, before * unit + l.replenished_m3);
        near(atmos_after * unit, atmos_before * unit + l.replenished_m3 + (evap - rain) * unit);
        check(std::abs(l.residual_m3) <= l.tolerance_m3);
        return count;
    }
    bool same(const RegionalState& a, const RegionalState& b) {
        return a.weather().tick() == b.weather().tick() && std::ranges::equal(a.soil(), b.soil()) &&
               std::ranges::equal(a.weather().water(), b.weather().water()) && std::ranges::equal(a.weather().flow(), b.weather().flow()) &&
               std::ranges::equal(a.weather().forcing_east(), b.weather().forcing_east()) &&
               std::ranges::equal(a.weather().forcing_north(), b.weather().forcing_north());
    }
}
