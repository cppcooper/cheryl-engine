#pragma once
#include "regional.h"
namespace ProcGenRegionalChecks {
    [[nodiscard]] ProcGen::WeatherModel passive();
    [[nodiscard]] ProcGen::ClimateBaseline baseline(ProcGen::Index w = 1, ProcGen::Index h = 1, ProcGen::Index seconds = 1);
    [[nodiscard]] ProcGen::RegionalState literal();
    [[nodiscard]] ProcGen::RegionalState generated();
    [[nodiscard]] std::size_t inspect(const ProcGen::RegionalState&, const ProcGen::RegionalAdvance&);
    [[nodiscard]] bool same(const ProcGen::RegionalState&, const ProcGen::RegionalState&);
}
