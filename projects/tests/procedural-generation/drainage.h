#pragma once

#include "elevation.h"

namespace ProcGen {
    enum class DrainageReason { dimensions, height_count, outlets, overflow, cells, storage, work, algorithm, version, index };
    struct DrainageFailure {
        FailureKind kind;
        DrainageReason reason;
        bool operator==(const DrainageFailure&) const = default;
    };
    template <class T> using DrainageResult = std::expected<T, DrainageFailure>;
    [[nodiscard]] std::string_view drainage_reason_text(DrainageReason reason);
    inline constexpr CaveBudgets drainage_hard_limits{65'536, 4 * 1024 * 1024, 2'000'000};
    inline constexpr Index no_receiver = UINT64_MAX;
    inline constexpr Index drainage_fixed_storage = fixed_storage_allowance + 8192;
    enum class DrainageAlgorithm : std::uint32_t { steepest_cardinal = 1 };
    struct DrainageOptions {
        DrainageAlgorithm algorithm = DrainageAlgorithm::steepest_cardinal;
        std::uint32_t version = 1;
        bool operator==(const DrainageOptions&) const = default;
    };
    // Synchronous borrowed input: heights.size()==domain.cells(). All heights,
    // outlets and their backing storage must remain stable during the call only.
    // Outlets are explicit flat indexes, any order; duplicates/out-of-range reject.
    // Empty outlet set is valid. Outside is absent, never an implicit outlet.
    struct DrainageInput {
        Domain domain;
        std::span<const Height> heights;
        std::span<const Index> outlets;
    };
    enum class TerminalKind : std::uint8_t { none, outlet, sink };
    struct DrainageCell {
        Index receiver = no_receiver;           // absent at terminals
        Index terminal = no_receiver;           // terminal CELL index = stable catchment identity
        Index contributing = 1;                 // own unit plus all upstream units, not discharge
        TerminalKind kind = TerminalKind::none; // this cell, not the destination's kind
        bool operator==(const DrainageCell&) const = default;
    };
    struct DrainageDescriptor {
        Domain domain;
        CaveBudgets budgets;
        DrainageOptions options;
        bool operator==(const DrainageDescriptor&) const = default;
    };
    struct DrainageRequirements {
        Index cells, storage_bytes, minimum_work, maximum_work;
    };
    struct DrainageMeasurements {
        Index cells = 0, links = 0, outlets = 0, sinks = 0;
        Index outlet_contributing = 0, sink_contributing = 0;
        Index largest_catchment = 0, maximum_contributing = 0;
        bool operator==(const DrainageMeasurements&) const = default;
    };
    // Owned exact input witness (Domain, original heights, outlet membership in
    // cells.kind) and complete analysis. x=axis0 fastest, index=x+width*y; same
    // abstract signed height ticks/datum as input. No RNG, sea inference or edits.
    // Copies deep-copy; returned views expire on move/assignment/destruction.
    // Moved-from values support destruction/assignment only.
    class Drainage {
        DrainageDescriptor descriptor_;
        std::vector<Height> source_;
        std::vector<DrainageCell> cells_;
        DrainageMeasurements measured_;
        Index work_;
        Drainage(
            DrainageDescriptor descriptor,
            std::vector<Height> source,
            std::vector<DrainageCell> cells,
            DrainageMeasurements measured,
            Index work
        );
        friend DrainageResult<Drainage> analyze_drainage(DrainageInput, CaveBudgets, DrainageOptions);
    public:
        [[nodiscard]] const DrainageDescriptor& descriptor() const { return descriptor_; }
        [[nodiscard]] std::span<const Height> source_heights() const { return source_; }
        [[nodiscard]] std::span<const DrainageCell> cells() const { return cells_; }
        [[nodiscard]] DrainageResult<DrainageCell> cell(Index index) const;
        [[nodiscard]] DrainageMeasurements measurements() const { return measured_; }
        [[nodiscard]] Index work() const { return work_; }
    };
    // Payload: 12288 fixed bytes (8192 outlet-validation bitset + metadata),
    // C*(sizeof(DrainageCell)+4 source+4 indegree+8 queue) = 12288+48*C here.
    // Excludes allocator/runtime overhead and preexisting caller data, not RSS.
    // Minimum work=1024+13*C-2*O; actual adds 2*links; finite upper adds 2*(C-O).
    [[nodiscard]] DrainageResult<DrainageRequirements> drainage_requirements(const Domain& domain, Index outlet_count);
    // V1: explicit outlets stop regardless of slope. Otherwise choose a strictly
    // lower cardinal neighbor of greatest original-height drop; equal drops pick
    // smallest row-major index. Signed64 differences cover full signed32 extremes.
    // Equal flats/local minima become sinks. FIFO Kahn accumulation then reverse
    // terminal propagation is O(C+O), no repeated full-chain walks or sorting.
    // Sufficient budgets affect admission only. No partial value on refusal;
    // allocation/programming exceptions propagate. Witness is not a save format.
    [[nodiscard]] DrainageResult<Drainage> analyze_drainage(DrainageInput input, CaveBudgets budgets, DrainageOptions options = {});
}
