#pragma once

#include "spill.h"

namespace ProcGen {
    enum class ChannelReason {
        dimensions,
        shape,
        outlets,
        source,
        topology,
        threshold,
        overflow,
        cells,
        nodes,
        edges,
        paths,
        storage,
        work,
        algorithm,
        version
    };
    struct ChannelFailure {
        FailureKind kind;
        ChannelReason reason;
        bool operator==(const ChannelFailure&) const = default;
    };
    template <class T> using ChannelResult = std::expected<T, ChannelFailure>;
    [[nodiscard]] std::string_view channel_reason_text(ChannelReason reason);
    enum class ChannelMeaning : std::uint32_t { ranked_effective_forest = 1 };
    // Synchronous borrowed view, exact C heights/records and nonempty explicit
    // outlets. heights must match records.original; never pair by size/ID alone.
    // Meaning is an explicitly supplied ranked cardinal outlet-reaching forest,
    // not an assertion that arbitrary records reproduce U10's minimax algorithm.
    // No externally supplied counts: accumulation always uses these actual parents.
    struct ChannelInput {
        Domain domain;
        std::span<const Height> heights;
        std::span<const SpillCell> routing;
        std::span<const Index> outlets;
        ChannelMeaning meaning = ChannelMeaning::ranked_effective_forest;
    };
    struct ChannelBudgets {
        Index cells, nodes, edges, path_points, storage_bytes, work;
        bool operator==(const ChannelBudgets&) const = default;
    };
    inline constexpr ChannelBudgets channel_hard_limits{65'536, 65'536, 65'535, 131'070, 12 * 1024 * 1024, 2'000'000};
    enum class ChannelAlgorithm : std::uint32_t { threshold_runs = 1 };
    struct ChannelOptions {
        ChannelAlgorithm algorithm = ChannelAlgorithm::threshold_runs;
        std::uint32_t version = 1;
        bool operator==(const ChannelOptions&) const = default;
    };
    struct ChannelDescriptor {
        Domain domain;
        Index minimum_contributing;
        ChannelMeaning meaning;
        ChannelBudgets budgets;
        ChannelOptions options;
        bool operator==(const ChannelDescriptor&) const = default;
    };
    inline constexpr std::uint8_t channel_selected = 1, channel_through_fill = 2;
    inline constexpr std::uint8_t channel_source = 1, channel_junction = 2, channel_outlet = 4;
    struct ChannelNode {
        Index cell;
        std::uint8_t roles; // zero selected incoming=source, >=2=junction, root=outlet; roles can combine
        bool operator==(const ChannelNode&) const = default;
    };
    struct ChannelEdge {
        Index from, to, offset, count, fill_points; // node IDs; inclusive flat path range; positive-depth points incl endpoints
        bool operator==(const ChannelEdge&) const = default;
    };
    struct ChannelMeasurements {
        Index cells = 0, outlets = 0, selected_cells = 0, selected_links = 0, selected_filled_cells = 0;
        Index nodes = 0, edges = 0, path_points = 0, terminal_contributing = 0;
        bool operator==(const ChannelMeasurements&) const = default;
    };
    struct ChannelUsage {
        Index work, storage_bytes;
    };
    struct ChannelRequirements {
        Index cells, base_storage_bytes, maximum_storage_bytes, minimum_work, maximum_work;
    };
    // Owned exact routing witness (including original/effective/depth/rank/outlet),
    // own-inclusive recomputed units, mask flags and compressed network. x-fast
    // coordinates/ticks/datum remain the input's. Copies deep-copy all vectors;
    // views expire on move/assignment/destruction. Moved-from supports only
    // assignment/destruction. No borrow, RNG or external accumulation is retained.
    class Channels {
        ChannelDescriptor descriptor_;
        std::vector<SpillCell> routing_;
        std::vector<Index> contributing_;
        std::vector<std::uint8_t> mask_;
        std::vector<ChannelNode> nodes_;
        std::vector<ChannelEdge> edges_;
        std::vector<Index> paths_;
        ChannelMeasurements measured_;
        ChannelUsage usage_;
        Channels(
            ChannelDescriptor,
            std::vector<SpillCell>,
            std::vector<Index>,
            std::vector<std::uint8_t>,
            std::vector<ChannelNode>,
            std::vector<ChannelEdge>,
            std::vector<Index>,
            ChannelMeasurements,
            ChannelUsage
        );
        friend ChannelResult<Channels> extract_channels(ChannelInput, Index, ChannelBudgets, ChannelOptions);
    public:
        [[nodiscard]] const ChannelDescriptor& descriptor() const { return descriptor_; }
        [[nodiscard]] std::span<const SpillCell> routing() const { return routing_; }
        [[nodiscard]] std::span<const Index> contributing() const { return contributing_; }
        [[nodiscard]] std::span<const std::uint8_t> mask() const { return mask_; }
        [[nodiscard]] std::span<const ChannelNode> nodes() const { return nodes_; }
        [[nodiscard]] std::span<const ChannelEdge> edges() const { return edges_; }
        [[nodiscard]] std::span<const Index> path_cells() const { return paths_; }
        [[nodiscard]] ChannelMeasurements measurements() const { return measured_; }
        [[nodiscard]] ChannelUsage usage() const { return usage_; }
    };
    // Base12288+66*C includes owned routing/units/mask and rank/map/degree scratch.
    // Actual peak adds16*N+40*E+8*P. Excludes prior inputs/runtime overhead, not RSS.
    // Known minimum work1024+14*C-O; exact adds2*L+3*N+2*E+2*P.
    [[nodiscard]] ChannelResult<ChannelRequirements> channel_requirements(const Domain&, Index outlet_count);
    // Positive threshold, inclusive units>=threshold. Above total cells is empty
    // success. Counts strictly increase downstream, so selected chains reach
    // explicit outlets. Compress degree-two runs; node IDs follow ascending cell,
    // edge IDs ascending non-outlet node ID. Paths include BOTH endpoints, follow
    // every actual receiver step, and retain fill crossings (mask flag2/edge count).
    // Isolated selected outlets are nodes without paths. IDs are result-scoped.
    // Validate source/depth/rank/cardinality/outlets and acyclicity, then recompute
    // contributions; this is not U9 accumulation or physical discharge/navigation.
    // Refusal publishes no partial result; allocation/programming exceptions escape.
    [[nodiscard]] ChannelResult<Channels>
    extract_channels(ChannelInput input, Index minimum_contributing, ChannelBudgets budgets, ChannelOptions options = {});
}
