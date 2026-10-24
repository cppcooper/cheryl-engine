#pragma once

#include <cheryl/core/game-framework/dialogue.h>

namespace CE::Dialogue {
    /** Explicit per-call budgets. Zero permits none; values above maximum_inspection_limits reject. */
    struct InspectionLimits {
        std::size_t nodes = 256;                  ///< Maximum authored nodes, independent of reachability.
        std::size_t choices = 2048;               ///< Maximum total directed edges, including parallel/conditional edges.
        std::size_t copied_id_bytes = 128 * 1024; ///< Sum of every ID byte copied into the report, counting repeated IDs.
        std::size_t storage_bytes = 1024 * 1024;  ///< Peak requested scratch/report payload; see inspect().
        std::size_t work = 128 * 1024 * 1024;     ///< Maximum deterministic work units; see inspect().
    };

    /** Absolute supported budgets; larger requests reject before input traversal or allocation. */
    inline constexpr InspectionLimits maximum_inspection_limits{4096, 65536, 4 * 1024 * 1024, 16 * 1024 * 1024, 1024 * 1024 * 1024};

    /** A refusal returns no partial report and does not mutate the definition or earlier reports. */
    enum class InspectionError {
        InvalidLimits, ///< At least one requested budget exceeds its absolute supported maximum.
        NodeLimit,     ///< Authored node count exceeds the node budget.
        ChoiceLimit,   ///< Total authored choice count exceeds the edge budget.
        CopiedIdLimit, ///< Repeated report ID bytes exceed their budget.
        StorageLimit,  ///< Requested peak payload exceeds the storage budget.
        WorkLimit      ///< The next charged operation would exceed the work budget.
    };

    /** Resources charged for a successful inspection; not wall time or resident process memory. */
    struct InspectionUsage {
        std::size_t copied_id_bytes = 0; ///< Includes definition/entry IDs and all three lists, without string terminators.
        std::size_t storage_bytes = 0;   ///< Peak requested scratch, vector slots, report object and ID bytes/terminators.
        std::size_t work = 0;            ///< Actual charged lookup/traversal/copy work, independent of supplied headroom.
    };

    /** Independently owned advisory topology. Lists preserve authored order, not traversal order.
     * All edges ignore conditions. These facts do not prove gameplay reachability,
     * effect history, termination or guaranteed completion. No definition owner is retained.
     */
    struct InspectionReport {
        DefinitionId definition;               ///< Exact compiled definition identity.
        std::uint64_t revision = 0;            ///< Exact compiled revision; not evidence of history/content authenticity.
        NodeId entry;                          ///< Compiled entry, which need not be the first authored node.
        std::size_t nodes = 0;                 ///< All authored nodes.
        std::size_t choices = 0;               ///< All authored choices, including repeated targets.
        std::size_t terminals = 0;             ///< Nodes explicitly compiled as NodeKind::Terminal.
        std::vector<NodeId> reachable;         ///< Structurally reachable from entry, including entry itself.
        std::vector<NodeId> unreachable;       ///< Complement of reachable, in authored order.
        std::vector<NodeId> no_terminal_route; ///< Nodes with no structural path to any terminal; cycles remain valid.
        InspectionUsage usage;                 ///< Charged resources for this result.
    };

    /** Iteratively inspects one frozen definition, borrowed only until return; safe for independent concurrent reads.
     * No Session, handlers, predicates, effects, paths, JSON or formatting are involved.
     * Terminals have a zero-edge route to themselves; reverse traversal seeds all terminals,
     * including unreachable ones. A terminal-free definition and explicit cycles are valid.
     *
     * Limits are checked before allocating the corresponding storage or copying IDs.
     * Storage counts requested index slots, report/vector objects, copied characters and
     * one terminator per string; allocator metadata/rounding, fixed stack and borrowed input
     * are excluded. It is a payload budget, not an allocator/process-memory guarantee.
     * Work charges each row/edge visit, initialized/prefix/copied scratch index,
     * lookup candidate and actually compared ID byte. Report assembly charges each
     * row visit, list append and copied ID byte/terminator. Fixed bookkeeping and
     * allocator internals are excluded; this is not a timing guarantee. Lookup is
     * O(N E L); iterative adjacency/traversal is O(N+E), plus copied/output storage.
     * std::bad_alloc and other allocation exceptions propagate; no caller value is changed.
     * @return An owned report or a typed budget refusal, never a usable partial report.
     */
    [[nodiscard]] CE_API std::expected<InspectionReport, InspectionError>
    inspect(const Definition& definition, const InspectionLimits& limits = {});
}
