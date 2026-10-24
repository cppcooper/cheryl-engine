#pragma once
#include "weather.h"
namespace ProcGen::WeatherDetail {
    // Exact existing source-match authority, charged into the caller's Work.
    WeatherResult<void> check_source(const ClimateBaseline&, const ClimateBaseline&, Work&);
    inline constexpr std::uint64_t curl_stage = 0x6375726c77696e64;
    WeatherResult<void> forcing(
        const ClimateBaseline&,
        const WeatherModel&,
        Index,
        std::span<double> potential,
        std::span<double> east,
        std::span<double> north,
        Work&
    );

    // Internal prevalidated scratch operator. No retained views; top-level owner
    // discards scratch after any error. Boundary/CFL checked before field writes.
    WeatherResult<void>
    wave(std::span<const FlowCell>, std::span<FlowCell>, const Domain&, const RegionalFrame&, double c, double dt, MoistureBoundary, Work&);
    void face_winds(std::span<const FlowCell>, const Domain&, MoistureBoundary, std::span<double>, std::span<double>, Work&);
}
