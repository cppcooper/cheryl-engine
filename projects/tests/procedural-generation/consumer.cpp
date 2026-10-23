#include "maze.h"
#include "checks.h"

#include <exception>
#include <iostream>

namespace {
    bool demonstrate(std::span<const ProcGen::Index> extents, ProcGen::StreamKey key, ProcGen::Budgets budgets) {
        const auto maze = ProcGen::generate(extents, key, budgets);
        if (!maze) {
            std::cerr << "Generation refused: " << ProcGen::reason_text(maze.error().reason) << '\n';
            return false;
        }
        const auto topology = ProcGenChecks::verify(*maze);
        std::cout << extents.size() << "D full orthogonal topology; extents=[";
        for (std::size_t axis = 0; axis < extents.size(); ++axis) {
            std::cout << (axis == 0 ? "" : ",") << extents[axis];
        }
        std::cout << "]; cells=" << topology.cells << "; edges=" << topology.edges << "; reached=" << topology.reached
                  << "; reciprocal/in-bounds/acyclic/coordinates=PASS\n";
        std::cout << "  edges by axis=[";
        for (std::size_t axis = 0; axis < extents.size(); ++axis) {
            std::cout << (axis == 0 ? "" : ",") << topology.axis_edges[axis];
        }
        std::cout << "]; algorithm=depth_first; version=" << maze->descriptor().version << "; seed=" << key.seed << "; stage=" << key.stage
                  << "; stream=" << key.stream << "; work=" << maze->usage().work << "; raw_draws=" << maze->usage().random_draws << '\n';
        return true;
    }
}

int main() {
    try {
        const ProcGen::Budgets budgets{.cells = 4096, .edges = 4095, .storage_bytes = 128 * 1024, .work = 1'000'000};
        const ProcGen::StreamKey key{.seed = 42, .stage = 0x6d617a65, .stream = 7};
        const std::array<ProcGen::Index, 2> plane{6, 4};
        const std::array<ProcGen::Index, 4> volume{3, 2, 2, 3};
        const bool plane_ok = demonstrate(plane, key, budgets);
        const bool volume_ok = demonstrate(volume, key, budgets);
        std::cout << "Consumer: " << (plane_ok + volume_ok) << " passed, " << (!plane_ok + !volume_ok)
                  << " failed, 0 skipped; reports full 4D graph.\n";
        return plane_ok && volume_ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Consumer exception: " << error.what() << '\n';
        return 1;
    }
}
