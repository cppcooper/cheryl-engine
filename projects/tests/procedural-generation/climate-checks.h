#pragma once
#include "climate.h"

namespace ProcGenClimateChecks {
    // Test/consumer-only <=16384 cells. Independent source/metric reductions,
    // normal geometry and response equations. Literal solar oracles additionally
    // live in climate-tests; no production solar helper is called here.
    void inspect(ProcGen::ClimateInput, const ProcGen::ClimateBaseline&);
}
