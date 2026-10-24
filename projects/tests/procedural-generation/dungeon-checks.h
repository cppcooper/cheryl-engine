#pragma once
#include "dungeon.h"

namespace ProcGenDungeonChecks {
    struct Report {
        ProcGen::Index floor = 0, components = 0;
    };
    // <=4096 cells. Independent half-open rectangle checks, cellwise painting of
    // closed corridor segments, and coordinate-pair cardinal BFS. No generator
    // scratch, neighbor helper, rejection placement or traversal is reused.
    [[nodiscard]] Report inspect(const ProcGen::Dungeon& dungeon);
}
