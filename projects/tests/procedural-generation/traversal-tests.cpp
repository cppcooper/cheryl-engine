#include "traversal-tests.h"
#include "traversal.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace AllocationProbe {
    extern thread_local bool denied; // existing test-only operator new in tests.cpp
}

namespace ProcGenTests {
    namespace {
        using namespace ProcGen;
        constexpr SearchLimits limits{{4096, 65536, 2 * 1024 * 1024, 2'000'000}, 4096, 8192, 16384};
        constexpr StreamKey key{42, 0x6d617a65, 7};
        constexpr Budgets generation_budget{4096, 4095, 128 * 1024, 1'000'000};
        constexpr Index maximum = std::numeric_limits<Index>::max();
        std::size_t assertions = 0;

        void check(bool ok, const char* expression, int line) {
            ++assertions;
            if (!ok) {
                throw std::runtime_error("line " + std::to_string(line) + ": " + expression);
            }
        }
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

        struct DenyAllocation {
            bool previous = AllocationProbe::denied;
            DenyAllocation() { AllocationProbe::denied = true; }
            ~DenyAllocation() { AllocationProbe::denied = previous; }
        };

        template <class T> void error_is(const Result<T>& result, FailureKind kind, Reason reason) {
            CHECK(!result);
            CHECK(result.error().kind == kind);
            CHECK(result.error().reason == reason);
        }
        template <class T> void invalid_is(const Result<T>& result, Reason reason) {
            error_is(result, FailureKind::invalid_input, reason);
        }
        template <class T> void refused_is(const Result<T>& result, Reason reason) {
            error_is(result, FailureKind::budget_refused, reason);
        }

        struct Edge {
            Index from;
            Index to;
            Index cost = 1;
        };

        // Reference input retains literal edges, separate from the mask view.
        struct Reference {
            std::size_t cells;
            std::vector<Edge> edges;
        };

        Coordinate decode(Index cell, std::span<const Index> extents) {
            Coordinate coordinate{};
            for (std::size_t axis = 0; axis < extents.size(); ++axis) {
                coordinate[axis] = cell % extents[axis];
                cell /= extents[axis];
            }
            return coordinate;
        }

        struct Fixture {
            Domain domain;
            Reference reference;
            std::vector<std::uint32_t> masks;
            std::vector<Index> costs;

            Fixture(std::initializer_list<Index> extents, std::initializer_list<Edge> edges)
            : domain(Domain::checked({extents.begin(), extents.size()}).value()),
              reference{static_cast<std::size_t>(domain.cells()), edges},
              masks(reference.cells, 0),
              costs(reference.cells * domain.dimensions(), 0) {
                for (const auto& edge : edges) {
                    if (edge.from >= domain.cells() || edge.to >= domain.cells() || edge.from == edge.to) {
                        throw std::logic_error("Invalid literal fixture endpoint");
                    }
                    const auto left = decode(edge.from, domain.extents());
                    const auto right = decode(edge.to, domain.extents());
                    std::optional<std::size_t> changed;
                    for (std::size_t axis = 0; axis < domain.dimensions(); ++axis) {
                        if (left[axis] != right[axis]) {
                            if (changed || std::max(left[axis], right[axis]) - std::min(left[axis], right[axis]) != 1) {
                                throw std::logic_error("Literal fixture edge is not an axis step");
                            }
                            changed = axis;
                        }
                    }
                    const auto axis = changed.value();
                    const auto direction = 2 * axis + (right[axis] > left[axis] ? 1U : 0U);
                    masks[static_cast<std::size_t>(edge.from)] |= std::uint32_t{1} << direction;
                    masks[static_cast<std::size_t>(edge.to)] |= std::uint32_t{1} << (direction ^ 1U);
                    costs[static_cast<std::size_t>(std::min(edge.from, edge.to)) * domain.dimensions() + axis] = edge.cost;
                }
            }

            MazeView view() const { return {domain, masks, costs}; }
            MazeView unit_view() const { return {domain, masks}; }
        };

        Reference reference_of(const Maze& maze) {
            const auto extents = maze.domain().extents();
            Reference reference{static_cast<std::size_t>(maze.domain().cells()), {}};
            // Pairwise coordinate comparison, independent of Domain::neighbor,
            // generator traversal and A*'s directional enumeration.
            for (Index left = 0; left < reference.cells; ++left) {
                for (Index right = left + 1; right < reference.cells; ++right) {
                    const auto a = decode(left, extents);
                    const auto b = decode(right, extents);
                    std::size_t differences = 0;
                    std::size_t changed = 0;
                    for (std::size_t axis = 0; axis < extents.size(); ++axis) {
                        if (a[axis] != b[axis]) {
                            ++differences;
                            changed = axis;
                        }
                    }
                    if (differences == 1 && b[changed] == a[changed] + 1 &&
                        (maze.passages()[static_cast<std::size_t>(left)] & (std::uint32_t{1} << (2 * changed + 1))) != 0) {
                        reference.edges.push_back({left, right, 1});
                    }
                }
            }
            return reference;
        }

        std::optional<Index> bfs(const Reference& graph, Index start, Index goal) {
            if (graph.cells > 64) {
                throw std::logic_error("Reference fixture exceeds bounded test size");
            }
            std::vector<std::optional<Index>> distance(graph.cells);
            std::vector<Index> queue{start};
            distance[static_cast<std::size_t>(start)] = 0;
            for (std::size_t head = 0; head < queue.size(); ++head) {
                const auto here = queue[head];
                for (const auto& edge : graph.edges) {
                    if (edge.from != here && edge.to != here) {
                        continue;
                    }
                    const auto there = edge.from == here ? edge.to : edge.from;
                    if (!distance[static_cast<std::size_t>(there)]) {
                        distance[static_cast<std::size_t>(there)] = distance[static_cast<std::size_t>(here)].value() + 1;
                        queue.push_back(there);
                    }
                }
            }
            return distance[static_cast<std::size_t>(goal)];
        }

        std::optional<Index> dijkstra(const Reference& graph, Index start, Index goal) {
            if (graph.cells > 64) {
                throw std::logic_error("Reference fixture exceeds bounded test size");
            }
            std::vector<std::optional<Index>> distance(graph.cells);
            std::vector<bool> done(graph.cells, false);
            distance[static_cast<std::size_t>(start)] = 0;
            // O(V^2) scan, no heap, heuristic, stale entries or production helpers.
            for (std::size_t iteration = 0; iteration < graph.cells; ++iteration) {
                std::optional<std::size_t> best;
                for (std::size_t cell = 0; cell < graph.cells; ++cell) {
                    if (!done[cell] && distance[cell] && (!best || distance[cell] < distance[*best])) {
                        best = cell;
                    }
                }
                if (!best) {
                    break;
                }
                done[*best] = true;
                for (const auto& edge : graph.edges) {
                    if (edge.from != *best && edge.to != *best) {
                        continue;
                    }
                    const auto next = static_cast<std::size_t>(edge.from == *best ? edge.to : edge.from);
                    if (edge.cost > maximum - distance[*best].value()) {
                        continue;
                    }
                    const auto cost = distance[*best].value() + edge.cost;
                    if (!distance[next] || cost < distance[next].value()) {
                        distance[next] = cost;
                    }
                }
            }
            return distance[static_cast<std::size_t>(goal)];
        }

        void verify_path(const Path& path, const Reference& graph, Index start, Index goal, Index expected_cost) {
            CHECK(!path.cells.empty());
            CHECK(path.cells.front() == start);
            CHECK(path.cells.back() == goal);
            std::vector<bool> seen(graph.cells, false);
            Index cost = 0;
            for (std::size_t i = 0; i < path.cells.size(); ++i) {
                const auto cell = path.cells[i];
                CHECK(cell < graph.cells);
                CHECK(!seen[static_cast<std::size_t>(cell)]);
                seen[static_cast<std::size_t>(cell)] = true;
                if (i != 0) {
                    const auto previous = path.cells[i - 1];
                    const auto edge = std::ranges::find_if(graph.edges, [&](const Edge& candidate) {
                        return (candidate.from == previous && candidate.to == cell) || (candidate.to == previous && candidate.from == cell);
                    });
                    CHECK(edge != graph.edges.end());
                    CHECK(edge->cost <= maximum - cost);
                    cost += edge->cost;
                }
            }
            CHECK(cost == expected_cost);
            CHECK(path.cost == cost);
        }

        void compare(MazeView graph, const Reference& reference, Index start, Index goal, bool weighted) {
            const auto expected = weighted ? dijkstra(reference, start, goal) : bfs(reference, start, goal);
            for (const auto heuristic : {Heuristic::zero, Heuristic::manhattan}) {
                const auto result = find_route(graph, start, goal, heuristic, limits).value();
                CHECK(result.path.has_value() == expected.has_value());
                if (expected) {
                    verify_path(result.path.value(), reference, start, goal, *expected);
                }
            }
        }

        void route_unit() {
            for (const auto& extents : std::array<std::vector<Index>, 2>{std::vector<Index>{5, 3}, {3, 2, 2, 3}}) {
                for (const auto seed : std::array<Index, 3>{0, 42, 999}) {
                    const auto maze = ProcGen::generate(extents, {seed, key.stage, key.stream}, generation_budget).value();
                    const auto original = maze;
                    const auto reference = reference_of(maze);
                    const auto goal = maze.domain().cells() - 1;
                    compare(maze_view(maze), reference, 0, goal, false);
                    const auto route = find_route(maze_view(maze), 0, goal, Heuristic::manhattan, limits).value();
                    const auto metrics = measure(maze_view(maze), 0, goal, route.path->cells, limits.budget).value();
                    CHECK(metrics.route->cost == bfs(reference, 0, goal).value());
                    CHECK(metrics.components == 1);
                    CHECK(metrics.cycle_rank == 0);
                    if (extents.size() == 4) {
                        bool fourth_axis = false;
                        for (std::size_t i = 1; i < route.path->cells.size(); ++i) {
                            const auto left = decode(route.path->cells[i - 1], extents);
                            const auto right = decode(route.path->cells[i], extents);
                            fourth_axis |= left[3] != right[3];
                        }
                        CHECK(fourth_axis);
                    }
                    CHECK(std::ranges::equal(maze.passages(), original.passages()));
                    CHECK(maze.descriptor() == original.descriptor());
                }
            }
        }

        Fixture cheaper_goal() {
            return {{2, 2}, {{0, 1, 10}, {0, 2, 1}, {2, 3, 1}, {3, 1, 1}}};
        }

        void route_weighted() {
            const auto fixture = cheaper_goal();
            compare(fixture.view(), fixture.reference, 0, 1, true);
            const std::vector<Index> expected{0, 2, 3, 1};
            const auto result = find_route(fixture.view(), 0, 1, Heuristic::manhattan, limits).value();
            CHECK(result.path->cells == expected);
            CHECK(result.path->cost == 3);   // first discovered goal had cost 10
            CHECK(result.usage.pushes == 5); // includes the improved goal candidate
            CHECK(result.usage.expansions == 4);
            const auto measured = measure(fixture.view(), 0, 1, result.path->cells, limits.budget).value();
            CHECK(measured.route->cost == 3);
            CHECK(!measured.tree_branches);

            const Fixture stale{{3, 2}, {{0, 1, 10}, {0, 3, 1}, {3, 4, 1}, {4, 1, 1}, {1, 2, 20}}};
            compare(stale.view(), stale.reference, 0, 2, true);
            const auto searched = find_route(stale.view(), 0, 2, Heuristic::zero, limits).value();
            CHECK(searched.path->cost == 23);
            CHECK(searched.usage.stale_pops == 1);
            CHECK(searched.usage.pushes == 6);
            CHECK(searched.usage.pops == 6);
            CHECK(searched.usage.expansions == 5);
            compare(stale.view(), stale.reference, 0, 5, true);
        }

        void route_cycles() {
            const Fixture square{{2, 2}, {{0, 1}, {1, 3}, {3, 2}, {2, 0}}};
            const std::vector<Index> pinned{0, 1, 3};
            for (const auto heuristic : {Heuristic::zero, Heuristic::manhattan}) {
                const auto first = find_route(square.unit_view(), 0, 3, heuristic, limits).value();
                const auto second = find_route(square.unit_view(), 0, 3, heuristic, limits).value();
                CHECK(first.path->cells == pinned);
                CHECK(second.path->cells == pinned);
                CHECK(first.usage.work == second.usage.work);
            }
            compare(square.unit_view(), square.reference, 0, 3, false);
            const Fixture disconnected{{3, 2}, {{0, 1}, {1, 2}, {3, 4}}};
            compare(disconnected.unit_view(), disconnected.reference, 0, 5, false);
            const auto no_path = find_route(disconnected.view(), 0, 5, Heuristic::zero, limits).value();
            CHECK(!no_path.path);
            CHECK(no_path.usage.expansions == 3);
            CHECK(no_path.usage.pops == 3);
            compare(disconnected.view(), disconnected.reference, 1, 1, false);
            const auto same = find_route(square.view(), 1, 1, Heuristic::manhattan, limits).value();
            CHECK(same.path->cells == std::vector<Index>{1});
            CHECK(same.path->cost == 0);
            CHECK(same.usage.expansions == 1);
            CHECK(same.usage.inspected_edges == 0);
            const Fixture singleton{{1}, {}};
            compare(singleton.view(), singleton.reference, 0, 0, false);
        }

        void route_limits() {
            const auto fixture = cheaper_goal();
            const auto result = find_route(fixture.view(), 0, 1, Heuristic::manhattan, limits).value();
            auto exact = limits;
            exact.expansions = result.usage.expansions;
            exact.queue_pushes = result.usage.pushes;
            exact.queue_entries = result.usage.peak_queue_entries;
            exact.budget = {4, 4, route_requirements(fixture.domain, exact.queue_entries)->storage_bytes, result.usage.work};
            CHECK(find_route(fixture.view(), 0, 1, Heuristic::manhattan, exact)->path->cells == result.path->cells);
            auto small = exact;
            --small.budget.storage_bytes;
            {
                const DenyAllocation guard;
                refused_is(find_route(fixture.view(), 0, 1, Heuristic::manhattan, small), Reason::storage);
            }
            small = exact;
            --small.budget.work;
            refused_is(find_route(fixture.view(), 0, 1, Heuristic::manhattan, small), Reason::work);
            small = exact;
            --small.expansions;
            refused_is(find_route(fixture.view(), 0, 1, Heuristic::manhattan, small), Reason::expansions);
            small = exact;
            --small.queue_pushes;
            refused_is(find_route(fixture.view(), 0, 1, Heuristic::manhattan, small), Reason::queue_pushes);
            small = exact;
            --small.queue_entries;
            refused_is(find_route(fixture.view(), 0, 1, Heuristic::manhattan, small), Reason::queue_entries);
            for (const auto reason :
                 {Reason::cells, Reason::edges, Reason::work, Reason::expansions, Reason::queue_entries, Reason::queue_pushes}) {
                small = limits;
                switch (reason) {
                    case Reason::cells:
                        small.budget.cells = 3;
                        break;
                    case Reason::edges:
                        small.budget.edges = 3;
                        break;
                    case Reason::work:
                        small.budget.work = 0;
                        break;
                    case Reason::expansions:
                        small.expansions = 0;
                        break;
                    case Reason::queue_entries:
                        small.queue_entries = 0;
                        break;
                    case Reason::queue_pushes:
                        small.queue_pushes = 0;
                        break;
                    default:
                        throw std::logic_error("Unexpected test limit");
                }
                const DenyAllocation guard;
                refused_is(find_route(fixture.view(), 0, 1, Heuristic::zero, small), reason);
            }
            const Fixture disconnected{{3, 2}, {{0, 1}, {1, 2}, {3, 4}}};
            small = limits;
            small.expansions = 2;
            refused_is(find_route(disconnected.view(), 0, 5, Heuristic::zero, small), Reason::expansions);
            CHECK(!find_route(disconnected.view(), 0, 5, Heuristic::zero, limits)->path);
        }

        void route_invalid() {
            Fixture line{{2}, {{0, 1}}};
            const auto original_masks = line.masks;
            const auto original_costs = line.costs;
            const auto call = [&](MazeView graph, Index start = 0, Index goal = 1, Heuristic heuristic = Heuristic::zero) {
                return find_route(graph, start, goal, heuristic, limits);
            };
            {
                const DenyAllocation guard;
                invalid_is(call(line.view(), 2), Reason::endpoint);
                invalid_is(call(line.view(), 0, maximum), Reason::endpoint);
                invalid_is(call(line.view(), 0, 1, static_cast<Heuristic>(2)), Reason::heuristic);
                invalid_is(call({line.domain, {}}), Reason::topology);
                invalid_is(call({line.domain, line.masks, {line.costs.data(), 1}}), Reason::cost);
            }
            line.masks[1] = 0;
            {
                const DenyAllocation guard;
                invalid_is(call(line.view()), Reason::topology);
            }
            line.masks = original_masks;
            line.masks[0] |= 1U; // lower boundary escape
            invalid_is(call(line.unit_view()), Reason::topology);
            line.masks = original_masks;
            line.masks[0] |= 1U << 5; // inactive axis
            invalid_is(call(line.unit_view()), Reason::topology);
            line.masks = original_masks;
            line.costs[0] = 0; // open edge has zero cost
            invalid_is(call(line.view()), Reason::cost);
            line.costs = original_costs;
            line.costs[1] = 1; // nonzero closed positive-axis slot
            invalid_is(call(line.view()), Reason::cost);
            line.costs = original_costs;
            CHECK(call(line.view())->path->cost == 1);
        }

        void route_overflow() {
            const Fixture maximum_edge{{2}, {{0, 1, maximum}}};
            for (const auto heuristic : {Heuristic::zero, Heuristic::manhattan}) {
                const auto result = find_route(maximum_edge.view(), 0, 1, heuristic, limits).value();
                verify_path(*result.path, maximum_edge.reference, 0, 1, maximum);
                const auto metrics = measure(maximum_edge.view(), 0, 1, result.path->cells, limits.budget).value();
                CHECK(metrics.route->cost == maximum);
            }
            const Fixture too_costly{{3}, {{0, 1, maximum}, {1, 2, 1}}};
            invalid_is(find_route(too_costly.view(), 0, 2, Heuristic::zero, limits), Reason::overflow);      // g addition
            invalid_is(find_route(too_costly.view(), 0, 2, Heuristic::manhattan, limits), Reason::overflow); // f addition
            const std::array<Index, 3> selected{0, 1, 2};
            invalid_is(measure(too_costly.view(), 0, 2, selected, limits.budget), Reason::overflow);
            const std::array<Index, 1> huge_extent{maximum};
            const auto huge = Domain::checked(huge_extent).value();
            const DenyAllocation guard;
            invalid_is(route_requirements(huge, 1), Reason::overflow);
            invalid_is(metric_requirements(huge), Reason::overflow);
            invalid_is(route_requirements(maximum_edge.domain, maximum), Reason::overflow);
        }

        void route_ownership() {
            const auto fixture = cheaper_goal();
            const auto masks = fixture.masks;
            const auto costs = fixture.costs;
            const auto old = find_route(fixture.view(), 0, 1, Heuristic::zero, limits).value();
            const auto copy = old;
            CHECK(old.path->cells.data() != copy.path->cells.data());
            const auto later = find_route(fixture.view(), 3, 0, Heuristic::manhattan, limits).value();
            CHECK(old.path->cells.data() != later.path->cells.data());
            CHECK(old.path->cells == copy.path->cells);
            auto small = limits;
            small.expansions = 1;
            refused_is(find_route(fixture.view(), 0, 1, Heuristic::zero, small), Reason::expansions);
            invalid_is(find_route(fixture.view(), maximum, 1, Heuristic::zero, limits), Reason::endpoint);
            CHECK(old.path->cells == copy.path->cells);
            bool allocation_exception = false;
            try {
                const DenyAllocation guard;
                static_cast<void>(find_route(fixture.view(), 0, 1, Heuristic::zero, limits));
            } catch (const std::bad_alloc&) {
                allocation_exception = true;
            }
            CHECK(allocation_exception);
            CHECK(old.path->cost == copy.path->cost);
            CHECK(fixture.masks == masks);
            CHECK(fixture.costs == costs);
            const auto outlives_input = [] {
                const Fixture temporary{{3}, {{0, 1}, {1, 2}}};
                return find_route(temporary.view(), 0, 2, Heuristic::zero, limits).value();
            }();
            const std::vector<Index> expected{0, 1, 2};
            CHECK(outlives_input.path->cells == expected);
        }

        void metrics_path() {
            const Fixture line{{4}, {{0, 1}, {1, 2}, {2, 3}}};
            const std::array<Index, 4> path{0, 1, 2, 3};
            const auto measured = measure(line.view(), 0, 3, path, limits.budget).value();
            CHECK(measured.active_cells == 4);
            CHECK(measured.edges == 3);
            CHECK(measured.reachable == 4);
            CHECK(measured.components == 1);
            CHECK(measured.cycle_rank == 0);
            CHECK(measured.degrees[1] == 2 && measured.degrees[2] == 2);
            CHECK(measured.dead_ends == 2);
            const RouteMeasurements expected_route{4, 3, 3};
            const TreeBranches empty{0, 0, 0, 0, 0};
            CHECK(measured.route == expected_route);
            CHECK(measured.tree_branches == empty);
            CHECK(measured.strict_labyrinth);
            const std::array<Index, 3> wrong_endpoint_path{1, 2, 3};
            const auto wrong_endpoint = measure(line.view(), 1, 3, wrong_endpoint_path, limits.budget).value();
            const TreeBranches endpoint_branch{1, 1, 1, 1, 1};
            CHECK(!wrong_endpoint.strict_labyrinth);
            CHECK(wrong_endpoint.tree_branches == endpoint_branch);
            const std::array<Index, 1> trivial{1};
            const auto same = measure(line.view(), 1, 1, trivial, limits.budget).value();
            CHECK(same.route->edges == 0 && same.route->cost == 0);
            CHECK(!same.strict_labyrinth);

            const Fixture singleton{{1}, {}};
            const std::array<Index, 1> one{0};
            const auto single = measure(singleton.view(), 0, 0, one, limits.budget).value();
            CHECK(single.strict_labyrinth);
            CHECK(single.active_cells == 1 && single.edges == 0 && single.components == 1);
            CHECK(single.degrees[0] == 1 && single.dead_ends == 0);
            CHECK(single.route->cells == 1 && single.route->cost == 0);
            CHECK(single.tree_branches == empty);
            const auto unselected = measure(singleton.view(), 0, 0, {}, limits.budget).value();
            CHECK(unselected.strict_labyrinth && !unselected.route && !unselected.tree_branches);

            // Literal winding 4D path; this fixture is not a new path generator.
            const Fixture winding{
                {2, 2, 2, 2},
                {{0, 1},
                 {1, 3},
                 {3, 2},
                 {2, 6},
                 {6, 7},
                 {7, 5},
                 {5, 4},
                 {4, 12},
                 {12, 13},
                 {13, 15},
                 {15, 14},
                 {14, 10},
                 {10, 11},
                 {11, 9},
                 {9, 8}}
            };
            const std::array<Index, 16> winding_path{0, 1, 3, 2, 6, 7, 5, 4, 12, 13, 15, 14, 10, 11, 9, 8};
            const auto full = measure(winding.view(), 0, 8, winding_path, limits.budget).value();
            CHECK(full.strict_labyrinth && full.active_cells == 16 && full.reachable == 16);
            CHECK(full.route->edges == 15 && full.route->cost == 15);
            CHECK(full.degrees[1] == 2 && full.degrees[2] == 14 && full.tree_branches == empty);
            compare(winding.view(), winding.reference, 0, 8, false);
        }

        Fixture branching() {
            return {{3, 2}, {{0, 1}, {1, 2}, {1, 4}, {4, 3}, {2, 5}}};
        }

        void metrics_branch() {
            const auto branch = branching();
            const std::array<Index, 3> solution{0, 1, 2};
            const auto measured = measure(branch.view(), 0, 2, solution, limits.budget).value();
            CHECK(measured.active_cells == 6 && measured.edges == 5 && measured.reachable == 6);
            CHECK(measured.components == 1 && measured.cycle_rank == 0); // a connected perfect maze
            CHECK(measured.degrees[1] == 3 && measured.degrees[2] == 2 && measured.degrees[3] == 1);
            CHECK(measured.dead_ends == 3 && !measured.strict_labyrinth);
            const RouteMeasurements expected_route{3, 2, 2};
            const TreeBranches expected{3, 3, 2, 2, 2};
            CHECK(measured.route == expected_route);
            CHECK(measured.tree_branches == expected);
            const Fixture deep{{4, 2}, {{0, 1}, {1, 2}, {2, 3}, {1, 5}, {5, 4}, {5, 6}, {6, 7}}};
            const std::array<Index, 4> path{0, 1, 2, 3};
            const auto lead = measure(deep.view(), 0, 3, path, limits.budget).value();
            const TreeBranches expected_deep{4, 4, 1, 2, 3};
            CHECK(lead.tree_branches == expected_deep);
            CHECK(lead.active_cells == 8 && lead.edges == 7 && lead.reachable == 8);
            CHECK(lead.degrees[1] == 4 && lead.degrees[2] == 2 && lead.degrees[3] == 2);
            CHECK(!lead.strict_labyrinth);
            compare(deep.view(), deep.reference, 0, 3, false);
        }

        void metrics_cycle() {
            const Fixture ladder{{3, 2}, {{0, 1}, {1, 2}, {3, 4}, {4, 5}, {0, 3}, {1, 4}, {2, 5}}};
            const std::array<Index, 3> path{0, 1, 2};
            const auto looped = measure(ladder.view(), 0, 2, path, limits.budget).value();
            CHECK(looped.active_cells == 6 && looped.edges == 7 && looped.reachable == 6);
            CHECK(looped.components == 1 && looped.cycle_rank == 2);
            CHECK(looped.dead_ends == 0);
            CHECK(looped.degrees[2] == 4 && looped.degrees[3] == 2); // two onward options at degree three
            CHECK(!looped.strict_labyrinth && !looped.tree_branches);
            CHECK(looped.route->cells == 3 && looped.route->cost == 2);
            compare(ladder.view(), ladder.reference, 0, 2, false);
            const Fixture disconnected{{3, 2}, {{0, 1}, {1, 2}, {3, 4}}};
            const auto split = measure(disconnected.view(), 0, 5, {}, limits.budget).value();
            CHECK(split.active_cells == 6 && split.edges == 3 && split.reachable == 3);
            CHECK(split.components == 3 && split.cycle_rank == 0);
            CHECK(split.degrees[0] == 1 && split.degrees[1] == 4 && split.degrees[2] == 1);
            CHECK(split.dead_ends == 4 && !split.strict_labyrinth);
            CHECK(!split.route && !split.tree_branches);
            const std::array<Index, 3> component_path{0, 1, 2};
            const auto selected_component = measure(disconnected.view(), 0, 2, component_path, limits.budget).value();
            CHECK(selected_component.route->cost == 2 && !selected_component.tree_branches);
            CHECK(!selected_component.strict_labyrinth); // selected component does not cover the active domain
        }

        void metrics_invalid() {
            const Fixture cycle{{3, 2}, {{0, 1}, {1, 4}, {4, 3}, {3, 0}, {1, 2}, {4, 5}}};
            const std::array<Index, 3> nonadjacent{0, 2, 5};
            invalid_is(measure(cycle.view(), 0, 5, nonadjacent, limits.budget), Reason::path);
            const std::array<Index, 6> repeated{0, 1, 0, 3, 4, 5};
            invalid_is(measure(cycle.view(), 0, 5, repeated, limits.budget), Reason::path);
            const std::array<Index, 3> outside{0, maximum, 5};
            invalid_is(measure(cycle.view(), 0, 5, outside, limits.budget), Reason::path);
            const std::array<Index, 2> wrong_end{0, 1};
            {
                const DenyAllocation guard;
                invalid_is(measure(cycle.view(), 0, 5, wrong_end, limits.budget), Reason::path);
                invalid_is(measure(cycle.view(), maximum, 5, {}, limits.budget), Reason::endpoint);
                invalid_is(measure({cycle.domain, {}}, 0, 5, {}, limits.budget), Reason::topology);
            }
            const auto empty = measure(cycle.view(), 0, 5, {}, limits.budget).value();
            CHECK(empty.reachable == 6 && !empty.route); // no supplied path is not a no-path claim
        }

        void metrics_limits() {
            const auto fixture = branching();
            const std::array<Index, 3> path{0, 1, 2};
            const auto original = measure(fixture.view(), 0, 2, path, limits.budget).value();
            const auto needed = metric_requirements(fixture.domain).value();
            const Budgets exact{6, 5, needed.storage_bytes, original.work};
            CHECK(measure(fixture.view(), 0, 2, path, exact)->tree_branches == original.tree_branches);
            auto small = exact;
            --small.storage_bytes;
            {
                const DenyAllocation guard;
                refused_is(measure(fixture.view(), 0, 2, path, small), Reason::storage);
            }
            small = exact;
            --small.work;
            refused_is(measure(fixture.view(), 0, 2, path, small), Reason::work);
            for (const auto reason : {Reason::cells, Reason::edges, Reason::work}) {
                small = exact;
                if (reason == Reason::cells) {
                    small.cells = 5;
                }
                if (reason == Reason::edges) {
                    small.edges = 4;
                }
                if (reason == Reason::work) {
                    small.work = 0;
                }
                const DenyAllocation guard;
                refused_is(measure(fixture.view(), 0, 2, path, small), reason);
            }
            const auto no_selected = measure(fixture.view(), 0, 2, {}, limits.budget).value();
            CHECK(no_selected.work == needed.minimum_work);
            small = exact;
            small.work = needed.minimum_work;
            CHECK(measure(fixture.view(), 0, 2, {}, small)->work == needed.minimum_work);
            bool allocation_exception = false;
            try {
                const DenyAllocation guard;
                static_cast<void>(measure(fixture.view(), 0, 2, path, limits.budget));
            } catch (const std::bad_alloc&) {
                allocation_exception = true;
            }
            CHECK(allocation_exception);
            CHECK(original.tree_branches->max_false_depth == 2);
            CHECK(original.route->cost == 2);
            const std::array<Index, 1> large_extent{1'000'001};
            const auto large = Domain::checked(large_extent).value();
            const DenyAllocation guard;
            CHECK(metric_requirements(large).has_value()); // planning itself is allocation-free
        }
    }

    Counts run_traversal_cases(std::string_view selection) {
        struct Case {
            std::string_view name;
            void (*run)();
        };
        const std::array<Case, 12> cases{
            {{"route_unit", route_unit},
             {"route_weighted", route_weighted},
             {"route_cycles", route_cycles},
             {"route_limits", route_limits},
             {"route_invalid", route_invalid},
             {"route_overflow", route_overflow},
             {"route_ownership", route_ownership},
             {"metrics_path", metrics_path},
             {"metrics_branch", metrics_branch},
             {"metrics_cycle", metrics_cycle},
             {"metrics_invalid", metrics_invalid},
             {"metrics_limits", metrics_limits}}
        };
        Counts counts;
        const auto beginning = assertions;
        for (const auto& test : cases) {
            if (selection != "all" && selection != test.name) {
                continue;
            }
            const auto before = assertions;
            try {
                test.run();
                ++counts.passed;
                std::cout << "PASS " << test.name << ": " << assertions - before << " assertions\n";
            } catch (const std::exception& error) {
                ++counts.failed;
                std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
            }
        }
        counts.assertions = assertions - beginning;
        return counts;
    }
}
