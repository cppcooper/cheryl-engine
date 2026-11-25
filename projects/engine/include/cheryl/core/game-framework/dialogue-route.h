#pragma once

#include <cheryl/core/game-framework/dialogue.h>

namespace CE::Dialogue {
    /** Independent per-call budgets; zero permits none. Requests above maximum_route_limits reject. */
    struct RouteLimits {
        std::size_t nodes = 256;                  ///< All authored nodes, including unreachable nodes.
        std::size_t choices = 2048;               ///< All authored edges, including self/parallel/conditional edges.
        std::size_t route_edges = 255;            ///< Maximum edges in the returned shortest route; never truncates search.
        std::size_t copied_id_bytes = 128 * 1024; ///< Repeated definition/node/choice ID bytes in the output.
        std::size_t storage_bytes = 1024 * 1024;  ///< Requested peak scratch/result payload, not process memory.
        std::size_t work = 128 * 1024 * 1024;     ///< Deterministic traversal/copy charges and indexed lookup reservations.
    };

    /** Absolute supported budgets; larger requests reject before traversal or allocation. */
    inline constexpr RouteLimits maximum_route_limits{4096, 65536, 4095, 4 * 1024 * 1024, 16 * 1024 * 1024, 1024 * 1024 * 1024};

    enum class RouteError {
        InvalidLimits, ///< A requested allowance exceeds its hard ceiling, including overflowing requests.
        NodeLimit,     ///< Too many authored nodes.
        ChoiceLimit,   ///< Too many authored choices.
        OutputLimit,   ///< The shortest route exceeds route_edges; not evidence of unreachability.
        CopiedIdLimit, ///< Repeated output ID bytes exceed their budget.
        StorageLimit,  ///< The next requested scratch/result payload exceeds its budget.
        WorkLimit,     ///< The next charged operation or lookup reservation exceeds its budget.
        UnknownFrom,   ///< Exact source ID is absent, including an empty ID.
        UnknownTo,     ///< Exact destination ID is absent, including an empty ID.
        Unreachable    ///< Complete structural search found no route between the known endpoints.
    };

    /** Charged resources of a successful call, independent of unused allowance. */
    struct RouteUsage {
        std::size_t copied_id_bytes = 0; ///< ID characters only; repeated IDs count repeatedly.
        std::size_t storage_bytes = 0;   ///< Peak requested scratch, result object, vector slots and ID bytes/terminators.
        std::size_t work = 0;            ///< Traversal/copy units plus conservative comparison/byte lookup reservations.
    };

    /** Independently owned shortest-edge structural advice. No definition or live selection authority is retained.
     * Every choice is an edge regardless of condition. This proves no gameplay availability,
     * effect history, termination or guaranteed completion; only Session can authorize playback.
     */
    struct Route {
        DefinitionId definition;       ///< Exact compiled identity.
        std::uint64_t revision = 0;    ///< Exact revision, not authentication of content or history.
        std::vector<NodeId> nodes;     ///< Ordered source through destination; one more element than choices.
        std::vector<ChoiceId> choices; ///< choices[i] belongs to nodes[i] and targets nodes[i+1].
        RouteUsage usage;              ///< Payload/work charged for this result.
    };

    /** Finds a shortest-edge route using bounded iterative breadth-first search.
     * First discovery wins, with FIFO nodes and authored choice order resolving equal-length ties.
     * Endpoints can be any compiled nodes, independent of entry/terminal status. Equal known
     * endpoints succeed with one node and zero choices, even in a terminal-free cycle.
     * The definition and endpoint arguments are borrowed only for the call. No Session,
     * predicates, effects, callbacks, paths or JSON are involved; independent reads are safe.
     * There is deliberately no nullable handle overload: callers must supply a live Definition.
     *
     * Limits reject in order: hard ceilings, authored counts/work, source/destination lookup,
     * minimum payload, scratch/search, then output counts/IDs/storage/copy work. A work/resource
     * refusal can precede endpoint/unreachable diagnosis; UnknownFrom precedes UnknownTo.
     * No partial route is returned, and the definition/previous results are never changed.
     *
     * Storage counts one 3*N size_t scratch allocation, sizeof(Route), requested vector slots,
     * ID characters and one terminator per copied string. Borrowed input, fixed stack, allocator
     * metadata/rounding and implementation-specific string capacity are excluded; this is not
     * a resident-memory guarantee. All count/product/byte arithmetic is checked before allocation.
     * Work charges each preflight row, initialized scratch/result slot, dequeued node, examined edge,
     * path/count/copy visit and copied ID byte/terminator. Each indexed node lookup reserves
     * (bit_width(N)+1)*(1+query_bytes) units before calling find_node: upper bounds on comparisons
     * and their byte work, not measured prefix comparisons. Fixed bookkeeping/allocator work is
     * excluded. Search is O(N+E) plus O((E+2)*log(N+1)*L) reserved lookup byte work and output copies.
     * std::bad_alloc/other allocation exceptions propagate; an impossible broken compiled-target
     * invariant throws std::logic_error. There is no timing, rollback or live authority guarantee.
     * @return An owned Route or typed refusal; route_edges never changes which path is shortest.
     */
    [[nodiscard]] CE_API std::expected<Route, RouteError>
    find_route(const Definition& definition, const NodeId& from, const NodeId& to, const RouteLimits& limits = {});
}
