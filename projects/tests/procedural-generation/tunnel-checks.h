#pragma once

#include "tunnels.h"

namespace ProcGenTunnelChecks {
    struct Counts {
        ProcGen::Index floor = 0;
        std::vector<std::vector<ProcGen::Index>> components;
    };
    // Test/consumer only, <=4096 cells. Pairwise coordinate-adjacency BFS,
    // independent of production Domain::neighbor and search/flood scratch.
    [[nodiscard]] Counts count(ProcGen::OccupancyView input);
    void inspect(ProcGen::OccupancyView input, const ProcGen::ConnectedCave& result);
    // <=256 cells. Independent dense Dijkstra reference for first attachment;
    // uses linear unsettled selection and coordinate-pair adjacency, no heap.
    [[nodiscard]] ProcGen::Index first_cost(ProcGen::OccupancyView input);
}
