#include "traversal.h"
#include "traversal-internal.h"

#include <bit>
#include <stdexcept>

namespace ProcGen {
    namespace {
        using namespace TraversalDetail;
        struct Cell {
            Index depth = 0;
            std::uint32_t flags = 0;
        };
        constexpr std::uint32_t reached = 1;
        constexpr std::uint32_t on_route = 2;
        constexpr std::uint32_t tree_seen = 4;
        static_assert(2 * sizeof(Measurements) + sizeof(Domain) + 512 <= fixed_storage_allowance);

        TraversalRequirements metric_storage(const Domain& domain) {
            if (domain.cells() > std::vector<Cell>{}.max_size() || domain.cells() > std::vector<Index>{}.max_size()) {
                invalid(Reason::overflow);
            }
            return {
                add(fixed_storage_allowance, multiply(domain.cells(), sizeof(Cell) + sizeof(Index))),
                multiply(domain.cells(), 7 + 4 * domain.dimensions())
            };
        }
    }

    Result<TraversalRequirements> metric_requirements(const Domain& domain) {
        try {
            return metric_storage(domain);
        } catch (const TraversalDetail::Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }

    Result<Measurements> measure(MazeView graph, Index entrance, Index goal, std::span<const Index> chosen_path, Budgets budget) {
        using namespace TraversalDetail;
        try {
            header(graph, entrance, goal);
            if (!chosen_path.empty() &&
                (chosen_path.size() > graph.domain.cells() || chosen_path.front() != entrance || chosen_path.back() != goal)) {
                invalid(Reason::path);
            }
            budget = effective(budget, metrics_hard_limits);
            const auto needed = metric_storage(graph.domain);
            admit(graph.domain, needed, budget);
            Work work(budget.work);
            const auto edges = validate(graph, budget, work);
            charge(work, 2 * graph.domain.cells());
            const auto size = static_cast<std::size_t>(graph.domain.cells());
            std::vector<Cell> cells(size);
            std::vector<Index> queue(size, 0);
            Measurements measured;
            measured.active_cells = graph.domain.cells();
            measured.edges = edges;
            for (Index cell = 0; cell < graph.domain.cells(); ++cell) {
                charge(work);
                ++measured.degrees[static_cast<std::size_t>(std::popcount(graph.passages[static_cast<std::size_t>(cell)]))];
            }
            measured.dead_ends = measured.degrees[1];
            const auto flood = [&](Index first) {
                std::size_t head = 0;
                std::size_t tail = 1;
                charge(work);
                queue[0] = first;
                cells[static_cast<std::size_t>(first)].flags |= reached;
                while (head < tail) {
                    charge(work);
                    const auto current = queue[head++];
                    for (std::size_t direction = 0; direction < 2 * graph.domain.dimensions(); ++direction) {
                        charge(work);
                        const auto edge = arc(graph, current, direction);
                        if (edge && (cells[static_cast<std::size_t>(edge->to)].flags & reached) == 0) {
                            charge(work);
                            cells[static_cast<std::size_t>(edge->to)].flags |= reached;
                            queue[tail++] = edge->to;
                        }
                    }
                }
                ++measured.components;
                return static_cast<Index>(tail);
            };
            measured.reachable = flood(entrance);
            for (Index cell = 0; cell < graph.domain.cells(); ++cell) {
                charge(work);
                if ((cells[static_cast<std::size_t>(cell)].flags & reached) == 0) {
                    static_cast<void>(flood(cell));
                }
            }
            const auto forest_edges = graph.domain.cells() - measured.components;
            if (edges < forest_edges) {
                throw std::logic_error("Undirected component edge invariant failed");
            }
            measured.cycle_rank = edges - forest_edges;
            Index route_cost = 0;
            for (std::size_t position = 0; position < chosen_path.size(); ++position) {
                charge(work);
                const auto current = chosen_path[position];
                if (current >= graph.domain.cells() || (cells[static_cast<std::size_t>(current)].flags & on_route) != 0) {
                    invalid(Reason::path);
                }
                cells[static_cast<std::size_t>(current)].flags |= on_route;
                if (position == 0) {
                    continue;
                }
                bool joined = false;
                for (std::size_t direction = 0; direction < 2 * graph.domain.dimensions(); ++direction) {
                    charge(work);
                    const auto edge = arc(graph, chosen_path[position - 1], direction);
                    if (edge && edge->to == current) {
                        route_cost = add(route_cost, edge->cost);
                        joined = true;
                        break;
                    }
                }
                if (!joined) {
                    invalid(Reason::path);
                }
            }
            if (!chosen_path.empty()) {
                measured.route = RouteMeasurements{chosen_path.size(), chosen_path.size() - 1, route_cost};
            }
            const bool connected_tree = measured.components == 1 && measured.cycle_rank == 0;
            measured.strict_labyrinth =
                connected_tree && (size == 1 ? entrance == goal
                                             : entrance != goal && measured.degrees[1] == 2 && measured.degrees[2] == size - 2 &&
                                                   std::popcount(graph.passages[static_cast<std::size_t>(entrance)]) == 1 &&
                                                   std::popcount(graph.passages[static_cast<std::size_t>(goal)]) == 1);
            if (connected_tree && !chosen_path.empty()) {
                TreeBranches branches{graph.domain.cells() - chosen_path.size(), graph.domain.cells() - chosen_path.size(), 0, 0, 0};
                std::size_t head = 0;
                std::size_t tail = 0;
                for (const auto cell : chosen_path) {
                    charge(work);
                    queue[tail++] = cell;
                    cells[static_cast<std::size_t>(cell)].flags |= tree_seen;
                }
                while (head < tail) {
                    charge(work);
                    const auto current = queue[head++];
                    const auto here = cells[static_cast<std::size_t>(current)];
                    if ((here.flags & on_route) == 0) {
                        branches.max_false_depth = std::max(branches.max_false_depth, here.depth);
                        if (std::popcount(graph.passages[static_cast<std::size_t>(current)]) == 1) {
                            ++branches.dead_ends;
                        }
                    }
                    for (std::size_t direction = 0; direction < 2 * graph.domain.dimensions(); ++direction) {
                        charge(work);
                        const auto edge = arc(graph, current, direction);
                        if (!edge) {
                            continue;
                        }
                        auto& next = cells[static_cast<std::size_t>(edge->to)];
                        if ((here.flags & on_route) != 0 && (next.flags & on_route) == 0) {
                            ++branches.branches;
                        }
                        if ((next.flags & tree_seen) == 0) {
                            charge(work);
                            next.flags |= tree_seen;
                            next.depth = add(here.depth, 1);
                            queue[tail++] = edge->to;
                        }
                    }
                }
                measured.tree_branches = branches;
            }
            measured.work = work.used();
            return measured;
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
}
