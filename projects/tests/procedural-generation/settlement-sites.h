#pragma once

#include "elevation.h"

namespace ProcGen {
    inline constexpr Index settlement_capacity = 16;
    inline constexpr CaveBudgets settlement_hard_limits{65'536, 4096, 20'000'000};
    enum class SettlementReason { dimensions, height_count, options, overflow, cells, storage, work };
    struct SettlementFailure {
        FailureKind kind;
        SettlementReason reason;
        bool operator==(const SettlementFailure&) const = default;
    };
    template <class T> using SettlementResult = std::expected<T, SettlementFailure>;
    struct SettlementOptions {
        Index requested = 4;
        Height sea_level = 0;
        Index maximum_slope = UINT32_MAX; // inclusive absolute cardinal difference in height ticks
        Index minimum_spacing = 1;       // inclusive Manhattan distance, must be positive
        bool operator==(const SettlementOptions&) const = default;
    };
    struct SettlementSite {
        Index index = 0, x = 0, y = 0, slope = 0;
        Height height = 0;
        bool operator==(const SettlementSite&) const = default;
    };
    struct SettlementRequirements {
        Index cells, interior_cells, storage_bytes, work;
    };
    // Private fixed-capacity value, no heap or retained input. Copy/move preserve
    // owned scalars; sites() views last until owner assignment/destruction.
    class SettlementSites {
        std::array<SettlementSite, settlement_capacity> sites_{};
        SettlementOptions options_;
        Index achieved_ = 0, work_ = 0, storage_ = 0;
        friend SettlementResult<SettlementSites>
        select_settlement_sites(const Domain&, std::span<const Height>, SettlementOptions, CaveBudgets);
    public:
        [[nodiscard]] std::span<const SettlementSite> sites() const {
            return {sites_.data(), static_cast<std::size_t>(achieved_)};
        }
        [[nodiscard]] SettlementOptions options() const { return options_; }
        [[nodiscard]] Index requested() const { return options_.requested; }
        [[nodiscard]] Index achieved() const { return achieved_; }
        [[nodiscard]] Index shortfall() const { return requested() - achieved_; }
        [[nodiscard]] Index work() const { return work_; }
        [[nodiscard]] Index storage_bytes() const { return storage_; }
    };
    // Positive checked 2D domains, requested<=16, slope<=UINT32_MAX, spacing>=1.
    // C=all cells, I=interior cells, R=requested. Exact charged work:
    // I*(5*R+R*(R-1)/2)+R, including empty rounds and spacing slots.
    // Accounted data bytes=sizeof(SettlementSites)+sizeof(SettlementSite):
    // fixed output plus one best-site scratch; excludes input, scalar loop/control
    // state, ABI call temporaries/runtime overhead; not RSS. No heap scratch.
    [[nodiscard]] SettlementResult<SettlementRequirements>
    settlement_requirements(const Domain& domain, SettlementOptions options);
    // Synchronous borrowed heights must match domain.cells() and stay stable
    // during the call. Exclude perimeter and heights<=sea, require slope<=limit.
    // Each round selects the smallest (maximum absolute cardinal difference,
    // row-major index) that meets spacing from every prior site. No RNG/sorting.
    // Difference uses signed64 before absolute value; signed32 extremes are valid.
    // Admit complete exact counts before reads; budget refusal returns no partial
    // value. Shortfall is successful exhaustion, not refusal. Never mutates input.
    [[nodiscard]] SettlementResult<SettlementSites>
    select_settlement_sites(const Domain& domain, std::span<const Height> heights, SettlementOptions options, CaveBudgets budgets);
    [[nodiscard]] SettlementResult<SettlementSites>
    select_settlement_sites(const Elevation& elevation, SettlementOptions options, CaveBudgets budgets);
}
