#include "maze.h"
#include "configuration.h"
#include "checks.h"
#include "traversal.h"

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
        const ProcGen::SearchLimits limits{{4096, 65536, 2 * 1024 * 1024, 2'000'000}, 4096, 8192, 16384};
        const auto graph = ProcGen::maze_view(*maze);
        const auto goal = maze->domain().cells() - 1;
        const auto route = ProcGen::find_route(graph, 0, goal, ProcGen::Heuristic::manhattan, limits);
        if (!route) {
            std::cerr << "Search refused: " << ProcGen::reason_text(route.error().reason) << '\n';
            return false;
        }
        if (!route->path) {
            std::cerr << "Search completed without an entrance-to-goal route\n";
            return false;
        }
        const auto measured = ProcGen::measure(graph, 0, goal, route->path->cells, limits.budget);
        if (!measured) {
            std::cerr << "Measurements refused: " << ProcGen::reason_text(measured.error().reason) << '\n';
            return false;
        }
        std::cout << "  search_version=" << ProcGen::search_version << "; route_cells=" << route->path->cells.size()
                  << "; route_edges=" << measured->route->edges << "; cost=" << route->path->cost
                  << "; expansions=" << route->usage.expansions << "; pushes=" << route->usage.pushes
                  << "; stale=" << route->usage.stale_pops << "; search_work=" << route->usage.work << '\n';
        std::cout << "  components=" << measured->components << "; reachable=" << measured->reachable
                  << "; cycle_rank=" << measured->cycle_rank << "; dead_ends=" << measured->dead_ends
                  << "; strict_labyrinth=" << (measured->strict_labyrinth ? "yes" : "no") << '\n';
        if (measured->tree_branches) {
            const auto& branches = *measured->tree_branches;
            std::cout << "  off_route_cells=" << branches.off_route_cells << "; attached_branches=" << branches.branches
                      << "; max_false_depth_edges=" << branches.max_false_depth << '\n';
        } else {
            std::cout << "  tree-only branch measurements do not apply\n";
        }
        return true;
    }

    bool demonstrate_configured(std::span<const ProcGen::Index> extents, ProcGen::StreamKey key, double value, ProcGen::Budgets budgets) {
        const auto maze = ProcGen::generate_configured(extents, key, {.traversability = value}, budgets);
        if (!maze) {
            std::cerr << "Configured generation refused: " << ProcGen::reason_text(maze.error().reason) << '\n';
            return false;
        }
        const auto topology = ProcGenChecks::verify(ProcGen::maze_view(*maze));
        const auto ends = maze->endpoints();
        const ProcGen::SearchLimits limits{{4096, 65536, 2 * 1024 * 1024, 2'000'000}, 4096, 8192, 16384};
        const auto search = ProcGen::find_route(ProcGen::maze_view(*maze), ends.entrance, ends.goal, ProcGen::Heuristic::manhattan, limits);
        if (!search || !search->path || search->path->cells.size() != maze->main_route().size() ||
            search->path->cost != maze->main_route().size() - 1) {
            std::cerr << "Configured route verification failed\n";
            return false;
        }
        const auto& measured = maze->measurements();
        std::cout << extents.size() << "D configured full topology; requested=" << value << "; ticks=" << maze->effective().ticks << "/"
                  << ProcGen::traversal_scale << "; cells=" << topology.cells << "; edges=" << topology.edges
                  << "; reached=" << topology.reached << "; endpoints=" << ends.entrance << "->" << ends.goal
                  << "; route_cells=" << measured.route->cells << "; cost=" << measured.route->cost
                  << "; off_route_cells=" << measured.tree_branches->off_route_cells << "; branches=" << measured.tree_branches->branches
                  << "; max_false_depth=" << measured.tree_branches->max_false_depth
                  << "; strict_labyrinth=" << (measured.strict_labyrinth ? "yes" : "no") << "\n  edges by axis=[";
        for (std::size_t axis = 0; axis < extents.size(); ++axis) {
            std::cout << (axis == 0 ? "" : ",") << topology.axis_edges[axis];
        }
        std::cout << "]; algorithm=serpentine_tree; version=" << maze->descriptor().version << "; seed=" << key.seed
                  << "; stage=" << key.stage << "; stream=" << key.stream << "; work_including_measurement=" << maze->usage().work
                  << "; raw_draws=" << maze->usage().random_draws << "; reciprocal/in-bounds/acyclic/coordinates/search=PASS\n";
        return true;
    }

}

int main() {
    try {
        const ProcGen::Budgets budgets{.cells = 4096, .edges = 4095, .storage_bytes = 128 * 1024, .work = 1'000'000};
        const ProcGen::StreamKey key{.seed = 42, .stage = 0x6d617a65, .stream = 7};
        const std::array<ProcGen::Index, 2> plane{6, 4};
        const std::array<ProcGen::Index, 4> volume{3, 2, 2, 3};
        unsigned passed = 0, failed = 0;
        const auto record = [&](bool ok) {
            if (ok) {
                ++passed;
            } else {
                ++failed;
            }
        };
        record(demonstrate(plane, key, budgets));
        record(demonstrate(volume, key, budgets));
        for (double value : {0.0, 0.5, 1.0}) {
            record(demonstrate_configured(plane, key, value, budgets));
            record(demonstrate_configured(volume, key, value, budgets));
        }
        std::cout << "Consumer: " << passed << " passed, " << failed << " failed, 0 skipped; reports full 4D graph.\n";
        return failed == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Consumer exception: " << error.what() << '\n';
        return 1;
    }
}
