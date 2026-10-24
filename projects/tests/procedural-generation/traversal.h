#pragma once

#include "maze.h"

namespace ProcGen {
    // Borrowed immutable orthogonal topology, independent of generation state.
    // ALL domain cells are active. Costs empty => unit; otherwise C*D entries:
    // costs[cell*D+axis] is that cell's positive-axis edge cost. Open slots are
    // in [1,UINT64_MAX], closed slots zero. Both incidences use the same slot.
    // Inputs must remain alive and unchanged throughout a call. No portals.
    struct MazeView {
        const Domain& domain;
        std::span<const std::uint32_t> passages;
        std::span<const Index> costs{};
    };

    [[nodiscard]] MazeView maze_view(const Maze& maze);

    enum class Heuristic { zero, manhattan };

    struct SearchLimits {
        Budgets budget;
        Index expansions;
        Index queue_entries;
        Index queue_pushes;
    };

    inline constexpr SearchLimits
        search_hard_limits{{1'000'000, 16'000'000, 64 * 1024 * 1024, 100'000'000}, 1'000'000, 1'000'000, 16'000'000};
    inline constexpr Budgets metrics_hard_limits = search_hard_limits.budget;
    inline constexpr std::uint32_t search_version = 1;
    inline constexpr std::uint32_t metrics_version = 1;

    struct TraversalRequirements {
        Index storage_bytes;
        Index minimum_work;
    };

    // Requested buffer payload + 4096 fixed bytes, not RSS. Existing input and
    // caller results/allocator/runtime overhead excluded. Helpers allocate no
    // buffers and do not apply hard caps; operations clamp limits to hard limits.
    [[nodiscard]] Result<TraversalRequirements> route_requirements(const Domain& domain, Index queue_entries);
    [[nodiscard]] Result<TraversalRequirements> metric_requirements(const Domain& domain);

    struct Path {
        std::vector<Index> cells; // entrance through goal inclusive; independent ownership
        Index cost;
    };

    struct SearchUsage {
        Index work = 0;
        Index expansions = 0;      // current settled pops, including the accepted goal
        Index inspected_edges = 0; // open incidences inspected after settlement
        Index pushes = 0;          // all entries, including improvements/duplicates
        Index pops = 0;
        Index stale_pops = 0;
        Index peak_queue_entries = 0;
    };

    struct SearchResult {
        std::optional<Path> path; // absent ONLY after the queue is completely searched
        SearchUsage usage;
    };

    // Only built-in consistent heuristics: zero and Manhattan with unit lower
    // bound on checked positive-cost axis edges. No arbitrary callbacks.
    // V1 order: smallest (f,g,cell), axis ascending/negative before positive;
    // equal-cost alternatives keep the first parent. Goal accepted on current
    // settled pop. Stale entries consume queue/work limits. Expected refusal
    // publishes no SearchResult/path; allocation/programming exceptions propagate.
    [[nodiscard]] Result<SearchResult> find_route(MazeView graph, Index entrance, Index goal, Heuristic heuristic, SearchLimits limits);

    struct RouteMeasurements {
        Index cells;
        Index edges;
        Index cost;
        bool operator==(const RouteMeasurements&) const = default;
    };

    struct TreeBranches {
        Index off_route_cells;
        Index off_route_edges;
        Index branches;        // edges attaching off-route subtrees to the selected path, including endpoints
        Index dead_ends;       // degree-one cells OFF the selected path
        Index max_false_depth; // unweighted edge distance from path attachment; zero when empty
        bool operator==(const TreeBranches&) const = default;
    };

    struct Measurements {
        Index active_cells = 0;
        Index edges = 0;
        Index reachable = 0; // from entrance, including entrance
        Index components = 0;
        Index cycle_rank = 0; // E-C+components over the entire active graph
        std::array<Index, 2 * max_dimensions + 1> degrees{};
        Index dead_ends = 0; // all degree-one cells, INCLUDING entrance/goal
        std::optional<RouteMeasurements> route;
        std::optional<TreeBranches> tree_branches; // connected tree AND an explicit path only
        bool strict_labyrinth = false;
        Index work = 0;
    };

    // Chosen path must be simple, adjacent and entrance-to-goal. Empty means
    // no path selected; it does NOT assert unreachability. Route cost is recomputed;
    // no shortest-path claim for arbitrary supplied paths. Tree-only measurements
    // are absent for cycles/disconnected inputs or no selected path.
    // Strict recognition is structural, independent of path selection: whole
    // graph is a simple path with selected distinct degree-one endpoints and
    // all others degree two; singleton accepts entrance=goal=0 (degree zero).
    // This is neither a no-reversal nor a subjective navigation guarantee.
    [[nodiscard]] Result<Measurements>
    measure(MazeView graph, Index entrance, Index goal, std::span<const Index> chosen_path, Budgets budget);
}
