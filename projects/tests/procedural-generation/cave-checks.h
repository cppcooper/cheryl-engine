#pragma once

#include "cave.h"

// Bounded test/consumer oracle, not production generation or budget accounting.
namespace ProcGenCaveChecks {
    struct Report {
        ProcGen::Index floor = 0;
        ProcGen::Index solid = 0;
        std::vector<std::vector<ProcGen::Index>> components;
    };
    // <=4096 cells. Independent coordinate-pair cardinal BFS and boundary/count
    // checks. Throws on invalid occupancy/domain; no generator labels reused.
    [[nodiscard]] Report inspect(const ProcGen::Cave& cave);
}
