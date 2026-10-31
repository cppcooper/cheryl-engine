#pragma once
#include "regional.h"
#include <algorithm>

namespace ProcGen {
    // Conservative synchronous CPU profile, not a latency or parallel-publication
    // guarantee. A caller shares one allowance across all regions in its batch.
    class RuntimeAllowance {
        Index remaining_work_, remaining_updates_;
        friend RegionalResult<RegionalAdvance>
        advance_runtime(const RegionalState&, Index, RuntimeAllowance&, WeatherWorkspace&);
    public:
        explicit constexpr RuntimeAllowance(Index work = 6'000'000, Index updates = 4) noexcept
        : remaining_work_(std::min(work, Index{6'000'000})), remaining_updates_(std::min(updates, Index{4})) {}
        [[nodiscard]] constexpr Index remaining_work() const noexcept { return remaining_work_; }
        [[nodiscard]] constexpr Index remaining_updates() const noexcept { return remaining_updates_; }
    };
    inline constexpr Index runtime_update_work = 1'500'000;
    inline RegionalResult<RegionalAdvance>
    advance_runtime(const RegionalState& old, Index ticks, RuntimeAllowance& allowance, WeatherWorkspace& scratch) {
        const auto& source = old.weather().baseline().descriptor();
        const auto& d = source.domain;
        const auto& m = old.weather().model();
        if (ticks != 1)
            return std::unexpected(RegionalFailure{FailureKind::budget_refused, RegionalReason::ticks, 0, {}});
        if (d.dimensions() != 2 || d.extents()[0] > 64 || d.extents()[1] > 48 || d.cells() > 3072)
            return std::unexpected(RegionalFailure{FailureKind::budget_refused, RegionalReason::cells, 0, {}});
        if (source.context.calendar.seconds_per_tick > 120 || m.forcing.count > 2)
            return std::unexpected(RegionalFailure{FailureKind::budget_refused, RegionalReason::substeps, 0, {}});
        if (!allowance.remaining_updates_ || allowance.remaining_work_ < runtime_update_work)
            return std::unexpected(RegionalFailure{FailureKind::budget_refused, RegionalReason::work, 0, {}});
        auto budget = regional_hard_limits;
        budget.work = runtime_update_work;
        budget.ticks = 1;
        budget.substeps_per_tick = 8;
        budget.cells = 3072;
        budget.storage_bytes = 4 * 1024 * 1024;
        auto result = advance_regional(old, 1, budget, scratch);
        // Both success and solver refusal consume real work. Profile refusals
        // above spend nothing. An allocation exception publishes nothing and
        // escapes; callers must not reuse the batch allowance after exceptions.
        allowance.remaining_work_ -= result ? result->state.usage().work : result.error().work;
        --allowance.remaining_updates_;
        if (result && scratch.retained_bytes() > 512 * 1024)
            return std::unexpected(RegionalFailure{FailureKind::budget_refused, RegionalReason::storage, result->state.usage().work, {}});
        return result;
    }
}
