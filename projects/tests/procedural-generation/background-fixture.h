#pragma once
#include "moisture-checks.h"
#include "regional-checks.h"
#include <array>
#include <stdexcept>

namespace ProcGenBackgroundFixture {
    inline ProcGen::RegionalState make(ProcGen::Index width=32, ProcGen::Index height=32,
                                      ProcGen::Index seconds=60, double metres=1000, bool flat=false) {
        using namespace ProcGen;
        if (width*height>3072) throw std::invalid_argument("finite background fixture cells");
        const auto count=static_cast<std::size_t>(width*height);
        auto elevation_result=generate_elevation(std::array<Index,2>{width,height}, {42,0x67656f6772617068,7},
                                                flat ? ElevationConfiguration{7,0,0} : ElevationConfiguration{7,-80,120},elevation_hard_limits);
        if (!elevation_result) throw std::runtime_error(std::string("background fixture elevation: ")+std::string(elevation_reason_text(elevation_result.error().reason)));
        const auto elevation=std::move(*elevation_result);
        std::array<ClimateSurface,3072> surface{};
        std::array<MoistureCell,3072> water{};
        std::array<FlowCell,3072> flow{};
        std::array<SoilCell,3072> soil{};
        for (std::size_t i=0;i<count;++i) {
            surface[i].land_response_seconds=3600; surface[i].water_response_seconds=14400;
            surface[i].roughness_metres=i%width==0 ? .01 : .8;
            surface[i].replenished_water_fraction=i%width==0 ? 1 : 0;
            if (i%width==1) { water[i].vapour_mm=9; water[i].cloud_mm=.5; }
            flow[i].east_m_s=1; soil[i].capacity_mm=2; soil[i].evaporation_mm_per_second=.000001;
        }
        const auto middle=count/2+static_cast<std::size_t>(width/2);
        surface[middle].finite_water_fraction=1; water[middle].finite_surface_mm=.02;
        auto baseline_result=derive_climate(elevation,std::span(surface).first(count),{1,1},
            ProcGenMoistureChecks::context(metres,metres,seconds),36,climate_hard_limits);
        if (!baseline_result) throw std::runtime_error(std::string("background fixture climate: ")+std::string(climate_reason_text(baseline_result.error().reason)));
        const auto baseline=std::move(*baseline_result);
        for (std::size_t i=0;i<count;++i) flow[i].surface_kelvin=280-.0065*baseline.cells()[i].z_metres;
        WeatherModel model;
        model.forcing.semantic={42,0x77696e64,7}; model.forcing.count=2;
        model.forcing.origin_east_metres=10000; model.forcing.origin_north_metres=20000;
        model.forcing.scales={WeatherScale{16,8,.25},WeatherScale{32,16,.1},WeatherScale{64,32,.05},WeatherScale{128,64,.02}};
        auto weather=start_weather(baseline,std::span(water).first(count),std::span(flow).first(count),model,weather_hard_limits);
        if (!weather) throw std::runtime_error(std::string("background fixture weather: ")+std::string(weather_reason_text(weather.error().reason)));
        auto regional=start_regional(*weather,std::span(soil).first(count),regional_hard_limits);
        if (!regional) throw std::runtime_error("background fixture regional reason="+std::to_string(static_cast<int>(regional.error().reason)));
        return std::move(*regional);
    }
}
