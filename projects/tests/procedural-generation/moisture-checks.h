#pragma once
#include "moisture.h"

namespace ProcGenMoistureChecks {
    // Test/consumer-only independent reductions, no production flux or capacity.
    void inspect(const ProcGen::MoistureState& before, const ProcGen::MoistureAdvance& after);
    ProcGen::ClimateContext context(double dx = 1, double dy = 1, ProcGen::Index seconds = 1);
    ProcGen::ClimateBaseline baseline(
        ProcGen::Index w,
        ProcGen::Index h,
        std::span<const ProcGen::Height>,
        std::span<const ProcGen::ClimateSurface>,
        const ProcGen::ClimateContext&,
        ProcGen::Index tick = 0
    );
    ProcGen::MoistureModel passive_model();
    struct Winds {
        std::vector<double> east, north;
        ProcGen::MoistureBoundary boundary;
        ProcGen::FaceWinds view() const { return {east, north, boundary}; }
    };
    Winds uniform_winds(const ProcGen::Domain&, const ProcGen::RegionalFrame&, double east, double north, ProcGen::MoistureBoundary);
}
