#include "fixtures.h"

#include <algorithm>
#include <iostream>
#include <limits>

namespace {
    using namespace CE::LivingWorld::Habitat::Fixture;
    using namespace ProcGen;
    template <class Value> void fails(const SettlementResult<Value>& value, FailureKind kind, SettlementReason reason) {
        check(!value && value.error() == SettlementFailure{kind, reason}, "exact producer refusal");
    }
    struct Grid {
        Domain domain;
        std::vector<Height> heights;
        std::vector<EligibilityCell> cells;
    };
    Grid grid(Index width = 5, Index height = 3) {
        const auto domain = take(Domain::checked(std::array<Index, 2>{width, height}));
        return {domain, std::vector<Height>(static_cast<std::size_t>(domain.cells()), 2),
            std::vector<EligibilityCell>(static_cast<std::size_t>(domain.cells()), EligibilityCell::eligible)};
    }
    EligibilityImage image(const Grid& input, EligibilityCoverage coverage = EligibilityCoverage::complete) {
        return take(EligibilityImage::make(input.domain, {1, 1, coverage}, input.cells, eligibility_hard_limits));
    }
    void literal_case() {
        auto input = grid();
        input.heights[2] = 3;
        std::ranges::fill(input.cells, EligibilityCell::ineligible);
        input.cells[7] = EligibilityCell::eligible;
        const auto eligibility = image(input);
        const SettlementOptions options{2, 0, 1, 2};
        const auto old = take(select_settlement_sites(input.domain, input.heights, options, settlement_hard_limits));
        check(old.achieved() == 2 && old.sites()[0].index == 6 && old.sites()[1].index == 8, "old dry shortlist unchanged");
        check(old.work() == 35 && old.storage_bytes() == sizeof(SettlementSites) + sizeof(SettlementSite), "old exact accounting");
        const auto selected = take(select_eligible_settlement_sites(input.domain, input.heights, eligibility, options, eligibility_hard_limits));
        check(selected.achieved() == 1 && selected.shortfall() == 1 && selected.sites()[0].index == 7,
            "eligibility before spacing finds wet middle");
        check(selected.work() == 56 && selected.coverage_complete() && selected.unknown_excluded() == 0, "exact new work/coverage");
    }
    void invalid_case() {
        auto input = grid();
        fails(EligibilityImage::make(input.domain, {2, 1, EligibilityCoverage::complete}, input.cells, eligibility_hard_limits),
            FailureKind::invalid_input, SettlementReason::eligibility);
        fails(EligibilityImage::make(input.domain, {1, 2, EligibilityCoverage::complete}, input.cells, eligibility_hard_limits),
            FailureKind::invalid_input, SettlementReason::eligibility);
        fails(EligibilityImage::make(input.domain, {1, 1, static_cast<EligibilityCoverage>(255)}, input.cells, eligibility_hard_limits),
            FailureKind::invalid_input, SettlementReason::eligibility);
        input.cells[7] = static_cast<EligibilityCell>(255);
        fails(EligibilityImage::make(input.domain, {1, 1, EligibilityCoverage::complete}, input.cells, eligibility_hard_limits),
            FailureKind::invalid_input, SettlementReason::eligibility);
        input.cells[7] = EligibilityCell::unknown;
        fails(EligibilityImage::make(input.domain, {1, 1, EligibilityCoverage::complete}, input.cells, eligibility_hard_limits),
            FailureKind::invalid_input, SettlementReason::eligibility);
        const auto unknown = image(input, EligibilityCoverage::incomplete);
        const auto selected = take(select_eligible_settlement_sites(input.domain, input.heights, unknown, {3, 0, 0, 1}, eligibility_hard_limits));
        check(!selected.coverage_complete() && selected.unknown_excluded() == 1 && selected.achieved() == 2, "Unknown excluded not dry or eligible");
        input.cells.pop_back();
        fails(EligibilityImage::make(input.domain, {1, 1, EligibilityCoverage::incomplete}, input.cells, eligibility_hard_limits),
            FailureKind::invalid_input, SettlementReason::eligibility);
        const auto other = grid(3, 5);
        const auto foreign = image(other);
        fails(select_eligible_settlement_sites(input.domain, input.heights, foreign, {}, eligibility_hard_limits),
            FailureKind::invalid_input, SettlementReason::eligibility);
    }
    void budgets_case() {
        const auto input = grid();
        const auto eligibility = image(input);
        const SettlementOptions options{2, 0, 0, 2};
        const auto needed = take(eligible_settlement_requirements(input.domain, options));
        const CaveBudgets exact{15, needed.storage_bytes, 56};
        const auto selected = take(select_eligible_settlement_sites(input.domain, input.heights, eligibility, options, exact));
        check(selected.work() == 56 && selected.storage_bytes() == needed.storage_bytes, "exact new boundary accepted");
        fails(select_eligible_settlement_sites(input.domain, input.heights, eligibility, options, {14, exact.storage_bytes, 56}),
            FailureKind::budget_refused, SettlementReason::cells);
        fails(select_eligible_settlement_sites(input.domain, input.heights, eligibility, options, {15, exact.storage_bytes - 1, 56}),
            FailureKind::budget_refused, SettlementReason::storage);
        fails(select_eligible_settlement_sites(input.domain, input.heights, eligibility, options, {15, exact.storage_bytes, 55}),
            FailureKind::budget_refused, SettlementReason::work);
        fails(select_eligible_settlement_sites(input.domain, input.heights, eligibility, options, {0, 0, 0}),
            FailureKind::budget_refused, SettlementReason::cells);
        const auto empty = take(select_eligible_settlement_sites(input.domain, input.heights, eligibility, {0, 0, 0, 1}, eligibility_hard_limits));
        check(empty.achieved() == 0 && empty.work() == 15, "zero requested still validates eligibility");
        const auto huge = take(Domain::checked(std::array<Index, 2>{UINT64_MAX, 1}));
        fails(EligibilityImage::requirements(huge), FailureKind::invalid_input, SettlementReason::overflow);
        fails(eligible_settlement_requirements(huge, {16, 0, 0, 1}), FailureKind::invalid_input, SettlementReason::overflow);
    }
    void ordering_case() {
        auto input = grid(6, 4);
        const auto eligibility = image(input);
        const auto selected = take(select_eligible_settlement_sites(input.domain, input.heights, eligibility, {4, 0, 0, 2}, eligibility_hard_limits));
        const std::array<Index, 4> expected{7, 9, 14, 16};
        check(selected.achieved() == expected.size(), "asymmetric tie count");
        for (std::size_t position = 0; position < expected.size(); ++position) {
            check(selected.sites()[position].index == expected[position], "literal row-major Manhattan spacing");
        }
        auto extremes = grid(3, 3);
        extremes.heights[4] = INT32_MAX;
        extremes.heights[1] = INT32_MIN;
        const auto extreme_image = image(extremes);
        const auto extreme = take(select_eligible_settlement_sites(extremes.domain, extremes.heights, extreme_image,
            {1, INT32_MIN, UINT32_MAX, 1}, eligibility_hard_limits));
        check(extreme.achieved() == 1 && extreme.sites()[0].slope == UINT32_MAX, "signed extremes use wide difference");
        const auto tiny = grid(2, 2);
        const auto tiny_image = image(tiny);
        const auto none = take(select_eligible_settlement_sites(tiny.domain, tiny.heights, tiny_image, {16, 0, 0, 1}, eligibility_hard_limits));
        check(none.achieved() == 0 && none.shortfall() == 16 && none.work() == 20, "no perimeter competition");
    }
}
int main(int argc, char** argv) {
    try {
        check(argc == 2, "one selector case required");
        const std::string_view name = argv[1];
        if (name == "literal") { literal_case(); }
        else if (name == "invalid") { invalid_case(); }
        else if (name == "budgets") { budgets_case(); }
        else if (name == "ordering") { ordering_case(); }
        else { throw std::runtime_error("unknown selector case"); }
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "habitat selector failure: " << failure.what() << '\n';
        return 1;
    }
}
