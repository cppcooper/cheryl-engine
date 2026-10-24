#include "weather.h"
#include "moisture-checks.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace ProcGen;
    namespace C = ProcGenMoistureChecks;
    struct Summary {
        double land, water, east, north, centre, rain;
    };
    Summary run(const char* label, const ClimateBaseline& b, WeatherModel model) {
        std::vector<MoistureCell> water(b.cells().size());
        std::vector<FlowCell> flow(b.cells().size());
        for (std::size_t i = 0; i < water.size(); ++i) {
            if (i % 8 == 1)
                water[i].vapour_mm = 9;
            flow[i].surface_kelvin = 280 - b.descriptor().context.model.lapse_kelvin_per_metre * b.cells()[i].z_metres;
            flow[i].east_m_s = 1;
        }
        water[27].finite_surface_mm = .02;
        auto state = start_weather(b, water, flow, model, weather_hard_limits).value();
        double rain = 0, external = 0, land = 0, sea = 0, moment = 0, mass = 0;
        std::cout << label << ": " << WeatherState::algorithm
                  << " v1/numerics1; model_s/tick=600; closed faces; semantic key={42,0x77696e64,7}\n";
        for (int interval = 0; interval < 4; ++interval) {
            auto result = advance_weather(state, b, 10, weather_hard_limits);
            if (!result)
                throw std::runtime_error(std::string(weather_reason_text(result.error().reason)));
            const auto& l = result->report.moisture.water;
            if (std::abs(l.residual_m3) > l.tolerance_m3)
                throw std::logic_error("water ledger");
            rain += l.rain_export_m3;
            external += l.replenished_m3;
            const auto& t = result->report.trajectory.back();
            land = 0;
            sea = 0;
            moment = 0;
            mass = 0;
            for (std::size_t i = 0; i < water.size(); ++i) {
                if (i % 8 == 0)
                    sea += result->state.flow()[i].surface_kelvin;
                else
                    land += result->state.flow()[i].surface_kelvin;
                mass += result->state.water()[i].vapour_mm;
                moment += result->state.water()[i].vapour_mm * static_cast<double>(i % 8);
            }
            land /= 42;
            sea /= 6;
            std::cout << "  tick=" << t.tick << " elapsed_s=" << (t.tick - 36) * 600 << " Ts_land/water_K=" << land << '/' << sea
                      << " A_reference_K=" << t.mean_reference_kelvin << " mean_wind_EN_m/s=" << t.mean_east_m_s << '/' << t.mean_north_m_s
                      << " max_m/s=" << t.maximum_speed_m_s << " potential_rms_m2/s2=" << t.potential_rms
                      << " vapour_centre_x_cells=" << (mass == 0 ? 0 : moment / mass)
                      << " substeps/tick=" << result->state.usage().substeps_per_tick << '\n';
            std::cout << "    interval water_m3 initial/final=" << l.initial_m3 << '/' << l.final_m3 << " external=" << l.replenished_m3
                      << " finite_to_air=" << l.finite_evaporation_m3 << " rain_export=" << l.rain_export_m3
                      << " residual=" << l.residual_m3 << " tolerance=" << l.tolerance_m3 << " boundary_in/out=0/0\n";
            state = std::move(result->state);
        }
        double east = 0, north = 0;
        for (auto f : state.flow()) {
            east += f.east_m_s;
            north += f.north_m_s;
        }
        if (!(rain > 0 && external > 0 && mass > 0))
            throw std::logic_error("missing evolved wet pulse and explicit supply");
        return {land, sea, east / 48, north / 48, moment / mass, rain};
    }
}
int main() {
    unsigned passed = 0;
    try {
        std::cout << std::fixed << std::setprecision(8);
        const auto e =
            generate_elevation(std::array<Index, 2>{8, 6}, {42, 0x67656f6772617068, 7}, {3, -80, 120}, elevation_hard_limits).value();
        auto context = C::context(2000, 2000, 600);
        std::vector<ClimateSurface> surface(48);
        for (std::size_t i = 0; i < 48; ++i) {
            surface[i].land_response_seconds = 3600;
            surface[i].water_response_seconds = 14400;
            surface[i].roughness_metres = i % 8 == 0 ? .01 : .8;
            if (i % 8 == 0)
                surface[i].replenished_water_fraction = 1;
        }
        surface[27].finite_water_fraction = 1;
        const auto b = derive_climate(e, surface, {1, 1}, context, 36, climate_hard_limits).value();
        WeatherModel m;
        m.forcing.semantic = {42, 0x77696e64, 7};
        m.forcing.origin_east_metres = 10000;
        m.forcing.origin_north_metres = 20000;
        m.forcing.count = 2;
        m.forcing.scales[0] = {4, 8, .25};
        m.forcing.scales[1] = {8, 16, .1};
        std::cout
            << "Retained U8 lattice_bilinear v1 8x6/pitch3/-80..120 ticks; explicit10m/tick datum0,2000m cells,40N,rows south.\n"
            << "Western water fraction is caller-declared; one finite0.02mm pool. Surface response land3600s/water14400s. Heights are not infinite walls.\n";
        const auto on = run("Thermal contrast/rotation/curl enabled", b, m);
        ++passed;
        auto same = m;
        same.use_surface_response = false;
        same.uniform_response_seconds = 3600;
        const auto uniform = run("Equal response times", b, same);
        ++passed;
        auto reverse = m;
        reverse.rotation_per_second = -m.rotation_per_second;
        const auto other = run("Reversed rotation", b, reverse);
        ++passed;
        auto no_thermal = m;
        no_thermal.thermal_enabled = false;
        const auto frozen = run("Thermal target disabled", b, no_thermal);
        ++passed;
        auto none = m;
        none.forcing.count = 0;
        const auto off = run("Curl forcing disabled", b, none);
        ++passed;
        if (!(std::abs(on.water - uniform.water) > .1 && std::abs(on.north - other.north) > .01 && std::abs(on.north - off.north) > 1e-6 &&
              std::abs(on.land - frozen.land) > 1 && std::abs(on.east - frozen.east) > 1e-4 && on.centre > 1))
            throw std::logic_error("missing measured response/rotation/forcing/transport effect");
        std::cout
            << "Comparison: water_Ts_delta_K=" << on.water - uniform.water
            << " reversed_rotation_north_delta_m/s=" << on.north - other.north
            << " thermal_disabled_east_delta_m/s=" << on.east - frozen.east << " forcing_north_delta_m/s=" << on.north - off.north
            << " rain_delta_m3=" << on.rain - off.rain << '\n'
            << "Artificial damped wave/thermal heuristic; potential is not hPa, rain is exported, no atmospheric mass/energy or forecast/visual claim.\nWeather consumer: "
            << passed << " passed, 0 failed, 0 skipped.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Weather consumer failed after " << passed << ": " << e.what() << '\n';
        return 1;
    }
}
