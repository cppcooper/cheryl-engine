#pragma once

#include "drainage.h"

namespace ProcGen {
    enum class SpillReason {
        dimensions,
        height_count,
        outlets,
        overflow,
        cells,
        storage,
        work,
        queue_entries,
        queue_pushes,
        algorithm,
        version,
        index
    };
    struct SpillFailure {
        FailureKind kind;
        SpillReason reason;
        bool operator==(const SpillFailure&) const = default;
    };
    template <class T> using SpillResult = std::expected<T, SpillFailure>;
    [[nodiscard]] std::string_view spill_reason_text(SpillReason reason);
    struct SpillBudgets {
        Index cells, storage_bytes, work, queue_entries, queue_pushes;
        bool operator==(const SpillBudgets&) const = default;
    };
    inline constexpr SpillBudgets spill_hard_limits{65'536, 4 * 1024 * 1024, 8'000'000, 65'536, 65'536};
    enum class SpillAlgorithm : std::uint32_t { priority_flood = 1 };
    struct SpillOptions {
        SpillAlgorithm algorithm = SpillAlgorithm::priority_flood;
        std::uint32_t version = 1;
        bool operator==(const SpillOptions&) const = default;
    };
    struct SpillCell {
        Height original = 0, effective = 0;
        Index depth = 0;            // effective-original, full signed32 span fits u64
        Index parent = no_receiver; // absent exactly at EXPLICIT outlet cells
        Index rank = no_receiver;   // unique settle order, parent rank strictly smaller
        Index outlet = no_receiver; // actual selected outlet cell index
        bool operator==(const SpillCell&) const = default;
    };
    struct SpillDescriptor {
        Domain domain;
        SpillBudgets budgets;
        SpillOptions options;
        bool operator==(const SpillDescriptor&) const = default;
    };
    struct SpillMeasurements {
        Index cells = 0, outlets = 0, filled_cells = 0, maximum_depth = 0, volume = 0;
        Height minimum_effective = 0, maximum_effective = 0;
        bool operator==(const SpillMeasurements&) const = default;
    };
    struct SpillUsage {
        Index work = 0, queue_pushes = 0, queue_pops = 0, peak_queue = 0, heap_extra_work = 0;
    };
    struct SpillRequirements {
        Index cells, queue_capacity, storage_bytes, minimum_work, maximum_work, maximum_volume;
    };
    // Independently owned original/effective/depth and parent/rank forest.
    // Original height values + parent-absent outlet membership + Domain/options
    // are the exact input witness. Same x-fast ticks/datum as the borrowed input;
    // no world, sea level, physical lake/rainfall or persisted recipe implied.
    // Copies deep-copy. Views expire on move/assignment/destruction; moved-from
    // values support destruction/assignment only. No borrow survives the call.
    class Spill {
        SpillDescriptor descriptor_;
        std::vector<SpillCell> cells_;
        SpillMeasurements measured_;
        SpillUsage usage_;
        Spill(SpillDescriptor descriptor, std::vector<SpillCell> cells, SpillMeasurements measured, SpillUsage usage);
        friend SpillResult<Spill> analyze_spill(DrainageInput, SpillBudgets, SpillOptions);
    public:
        [[nodiscard]] const SpillDescriptor& descriptor() const { return descriptor_; }
        [[nodiscard]] std::span<const SpillCell> cells() const { return cells_; }
        [[nodiscard]] SpillResult<SpillCell> cell(Index index) const;
        [[nodiscard]] SpillMeasurements measurements() const { return measured_; }
        [[nodiscard]] SpillUsage usage() const { return usage_; }
    };
    // Q=min(queue_entries,C). Requested peak12288+40*C+8*Q, not RSS; fixed includes
    // 8192 outlet validation bits and metadata. Nonempty distinct outlets required;
    // Q>=outlet_count is necessary, not a guarantee the frontier will fit.
    // Minimum1024+12*C+Q; actual adds heap_extra_work (replacement/comparison/swap).
    // Finite upper adds C+5*C*ceil(log2(Q)). Maximum volume=C*(2^32-1), checked.
    [[nodiscard]] SpillResult<SpillRequirements> spill_requirements(const Domain&, Index outlet_count, Index queue_entries);
    // Reuses only U9's synchronous Domain/heights/outlets VIEW. U10 requires >=1
    // explicit outlet; duplicates/out-of-range reject. Outside is absent. Minimax
    // effective(cell)=min over cardinal paths to outlets of max ORIGINAL heights
    // on that path, including both endpoints. No lowering; outlet height unchanged.
    // Priority flood v1 discovers once, frontier key=(effective,index), neighbor
    // order x-,x+,y-,y+. Parent was already popped; ranks give acyclic plateau
    // routing independent of U9 strict descent. This is not shortest-path routing.
    // Sufficient budgets affect content admission only; exhaustion publishes no
    // partial value. Allocation/programming exceptions propagate; inputs unchanged.
    [[nodiscard]] SpillResult<Spill> analyze_spill(DrainageInput input, SpillBudgets budgets, SpillOptions options = {});
}
