#pragma once
#include "moisture.h"

// Internal shared CPU step. Callers validate sizes, model/frame/winds, dt and
// actual outgoing/incoming fractions before entry. No retained view/clock owner.
namespace ProcGen::MoistureDetail {
    double capacity(double local_kelvin, const MoistureModel&);
    double local_temperature(double reference, const ClimateBaseline&, std::size_t);
    bool valid_cell(const MoistureCell&, const ClimateBaseline&, std::size_t, bool initial);
    std::pair<double, double> fractions(FaceWinds, const Domain&, const RegionalFrame&, Index, double dt);
    MoistureResult<void> substep(
        std::span<const MoistureCell>,
        std::span<MoistureCell>,
        const ClimateBaseline&,
        const MoistureModel&,
        FaceWinds,
        std::span<const double> outgoing,
        double dt,
        std::span<MoistureTransfer>,
        Work&
    );
}
