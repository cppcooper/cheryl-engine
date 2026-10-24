#pragma once
#include "spill.h"

namespace ProcGenSpillChecks {
    // <=512 cells. Independent threshold-connectivity BFS at original levels,
    // then traces EVERY actual parent chain; no priority flood helper reused.
    void inspect(ProcGen::DrainageInput input, const ProcGen::Spill& output);
}
