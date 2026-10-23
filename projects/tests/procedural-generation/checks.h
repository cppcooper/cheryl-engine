#pragma once

#include "maze.h"

// Test/consumer support only; independent of the generator's traversal.
namespace ProcGenChecks {
    struct Topology {
        ProcGen::Index cells;
        ProcGen::Index edges;
        ProcGen::Index reached;
        std::array<ProcGen::Index, ProcGen::max_dimensions> axis_edges;
    };

    // Bounded to 4096 cells. Throws with a diagnostic if any invariant fails.
    // Uses mixed-radix decoding, explicit edges, union/find and breadth-first
    // reachability rather than repeating the generator's depth-first algorithm.
    [[nodiscard]] Topology verify(const ProcGen::Maze& maze);
}
