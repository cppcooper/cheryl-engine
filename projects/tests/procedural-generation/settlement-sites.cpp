#include "settlement-sites.h"

#include <algorithm>
#include <limits>

namespace ProcGen {
    namespace {
        SettlementFailure invalid(SettlementReason reason) { return {FailureKind::invalid_input, reason}; }
        SettlementFailure refused(SettlementReason reason) { return {FailureKind::budget_refused, reason}; }
        Index difference(Height a, Height b) {
            const auto delta = static_cast<std::int64_t>(a) - static_cast<std::int64_t>(b);
            return static_cast<Index>(delta < 0 ? -delta : delta);
        }
        Index distance(Index a, Index b) { return a > b ? a - b : b - a; }
    }
    SettlementResult<SettlementRequirements> settlement_requirements(const Domain& domain, SettlementOptions options) {
        if (domain.dimensions() != 2) {
            return std::unexpected(invalid(SettlementReason::dimensions));
        }
        if (options.requested > settlement_capacity || options.maximum_slope > UINT32_MAX || options.minimum_spacing == 0) {
            return std::unexpected(invalid(SettlementReason::options));
        }
        const auto width = domain.extents()[0], height = domain.extents()[1];
        const Index interior = width < 3 || height < 3 ? 0 : (width - 2) * (height - 2);
        const auto r = options.requested;
        const auto units = 5 * r + (r == 0 ? 0 : r * (r - 1) / 2);
        if (units != 0 && interior > (std::numeric_limits<Index>::max() - r) / units) {
            return std::unexpected(invalid(SettlementReason::overflow));
        }
        return SettlementRequirements{domain.cells(), interior, sizeof(SettlementSites) + sizeof(SettlementSite), interior * units + r};
    }
    SettlementResult<SettlementSites> select_settlement_sites(
        const Domain& domain, std::span<const Height> heights, SettlementOptions options, CaveBudgets budgets
    ) {
        const auto needed = settlement_requirements(domain, options);
        if (!needed) {
            return std::unexpected(needed.error());
        }
        if (heights.size() != needed->cells) {
            return std::unexpected(invalid(SettlementReason::height_count));
        }
        if (needed->cells > std::min(budgets.cells, settlement_hard_limits.cells)) {
            return std::unexpected(refused(SettlementReason::cells));
        }
        if (needed->storage_bytes > std::min(budgets.storage_bytes, settlement_hard_limits.storage_bytes)) {
            return std::unexpected(refused(SettlementReason::storage));
        }
        if (needed->work > std::min(budgets.work, settlement_hard_limits.work)) {
            return std::unexpected(refused(SettlementReason::work));
        }
        SettlementSites result;
        result.options_ = options;
        result.storage_ = needed->storage_bytes;
        const auto width = domain.extents()[0], height = domain.extents()[1];
        for (Index round = 0; round < options.requested; ++round) {
            SettlementSite best;
            best.index = UINT64_MAX;
            if (needed->interior_cells != 0) {
                for (Index y = 1; y < height - 1; ++y) {
                    for (Index x = 1; x < width - 1; ++x) {
                        const auto index = x + width * y;
                        const auto center = heights[static_cast<std::size_t>(index)];
                        const auto slope = std::max(
                            std::max(difference(center, heights[static_cast<std::size_t>(index - 1)]),
                                     difference(center, heights[static_cast<std::size_t>(index + 1)])),
                            std::max(difference(center, heights[static_cast<std::size_t>(index - width)]),
                                     difference(center, heights[static_cast<std::size_t>(index + width)]))
                        );
                        result.work_ += 5;
                        bool eligible = center > options.sea_level && slope <= options.maximum_slope;
                        for (Index slot = 0; slot < round; ++slot) {
                            ++result.work_;
                            if (slot < result.achieved_) {
                                const auto& prior = result.sites_[static_cast<std::size_t>(slot)];
                                if (distance(x, prior.x) + distance(y, prior.y) < options.minimum_spacing) {
                                    eligible = false;
                                }
                            }
                        }
                        // Row-major visitation is the explicit secondary key.
                        if (eligible && (best.index == UINT64_MAX || slope < best.slope)) {
                            best = {index, x, y, slope, center};
                        }
                    }
                }
            }
            ++result.work_;
            if (best.index != UINT64_MAX) {
                result.sites_[static_cast<std::size_t>(result.achieved_++)] = best;
            }
        }
        return result;
    }
    SettlementResult<SettlementSites>
    select_settlement_sites(const Elevation& elevation, SettlementOptions options, CaveBudgets budgets) {
        return select_settlement_sites(elevation.descriptor().domain, elevation.heights(), options, budgets);
    }

    namespace {
        bool add_checked(Index left, Index right, Index& result) {
            if (left > UINT64_MAX - right) { return false; }
            result = left + right;
            return true;
        }
        bool multiply_checked(Index left, Index right, Index& result) {
            if (right != 0 && left > UINT64_MAX / right) { return false; }
            result = left * right;
            return true;
        }
        SettlementResult<void> admit_eligibility(SettlementRequirements needed, CaveBudgets budget) {
            if (needed.cells > std::min(budget.cells, eligibility_hard_limits.cells)) {
                return std::unexpected(refused(SettlementReason::cells));
            }
            if (needed.storage_bytes > std::min(budget.storage_bytes, eligibility_hard_limits.storage_bytes)
                || needed.storage_bytes > std::numeric_limits<std::size_t>::max()) {
                return std::unexpected(refused(SettlementReason::storage));
            }
            if (needed.work > std::min(budget.work, eligibility_hard_limits.work)) {
                return std::unexpected(refused(SettlementReason::work));
            }
            return {};
        }
        bool valid_cell(EligibilityCell cell) {
            return cell == EligibilityCell::eligible || cell == EligibilityCell::ineligible || cell == EligibilityCell::unknown;
        }
    }
    EligibilityImage::EligibilityImage(Domain domain, EligibilityPolicy policy, std::span<const EligibilityCell> cells)
    : domain_(std::move(domain)), policy_(policy), cells_(cells.begin(), cells.end()) {}

    SettlementResult<SettlementRequirements> EligibilityImage::requirements(const Domain& domain) {
        if (domain.dimensions() != 2) { return std::unexpected(invalid(SettlementReason::dimensions)); }
        Index bytes = 0, work = 0;
        if (!multiply_checked(domain.cells(), sizeof(EligibilityCell), bytes)
            || !add_checked(bytes, sizeof(EligibilityImage), bytes)
            || !multiply_checked(domain.cells(), 2, work)) {
            return std::unexpected(invalid(SettlementReason::overflow));
        }
        return SettlementRequirements{domain.cells(), 0, bytes, work};
    }
    SettlementResult<EligibilityImage> EligibilityImage::make(const Domain& domain,
        EligibilityPolicy policy, std::span<const EligibilityCell> cells, CaveBudgets budgets) {
        const auto needed = requirements(domain);
        if (!needed) { return std::unexpected(needed.error()); }
        if (policy.version != 1 || policy.policy != 1
            || (policy.coverage != EligibilityCoverage::complete && policy.coverage != EligibilityCoverage::incomplete)
            || cells.size() != needed->cells) {
            return std::unexpected(invalid(SettlementReason::eligibility));
        }
        const auto admitted = admit_eligibility(*needed, budgets);
        if (!admitted) { return std::unexpected(admitted.error()); }
        for (const auto cell : cells) {
            if (!valid_cell(cell) || (policy.coverage == EligibilityCoverage::complete && cell == EligibilityCell::unknown)) {
                return std::unexpected(invalid(SettlementReason::eligibility));
            }
        }
        return EligibilityImage(domain, policy, cells);
    }
    SettlementResult<SettlementRequirements> eligible_settlement_requirements(const Domain& domain, SettlementOptions options) {
        const auto terrain = settlement_requirements(domain, options);
        if (!terrain) { return std::unexpected(terrain.error()); }
        const auto image = EligibilityImage::requirements(domain);
        if (!image) { return std::unexpected(image.error()); }
        Index bytes = 0, work = 0;
        if (!add_checked(image->storage_bytes, sizeof(EligibleSettlementSites) + sizeof(SettlementSite), bytes)
            || !multiply_checked(terrain->interior_cells, options.requested, work)
            || !add_checked(work, domain.cells(), work) || !add_checked(work, terrain->work, work)) {
            return std::unexpected(invalid(SettlementReason::overflow));
        }
        return SettlementRequirements{domain.cells(), terrain->interior_cells, bytes, work};
    }
    SettlementResult<EligibleSettlementSites> select_eligible_settlement_sites(const Domain& domain,
        std::span<const Height> heights, const EligibilityImage& eligibility, SettlementOptions options, CaveBudgets budgets) {
        const auto needed = eligible_settlement_requirements(domain, options);
        if (!needed) { return std::unexpected(needed.error()); }
        if (heights.size() != needed->cells) { return std::unexpected(invalid(SettlementReason::height_count)); }
        if (eligibility.domain() != domain || eligibility.cells().size() != needed->cells) {
            return std::unexpected(invalid(SettlementReason::eligibility));
        }
        const auto admitted = admit_eligibility(*needed, budgets);
        if (!admitted) { return std::unexpected(admitted.error()); }
        EligibleSettlementSites result;
        result.policy_ = eligibility.policy();
        result.selection_.options_ = options;
        result.storage_ = needed->storage_bytes;
        const auto width = domain.extents()[0], height = domain.extents()[1];
        for (Index index = 0; index < needed->cells; ++index) {
            const auto cell = eligibility.cells()[static_cast<std::size_t>(index)];
            if (!valid_cell(cell)) { return std::unexpected(invalid(SettlementReason::eligibility)); }
            if (cell == EligibilityCell::unknown && index % width != 0 && index % width + 1 < width
                && index / width != 0 && index / width + 1 < height) { ++result.unknown_; }
            ++result.work_;
        }
        for (Index round = 0; round < options.requested; ++round) {
            SettlementSite best;
            best.index = UINT64_MAX;
            if (needed->interior_cells != 0) {
                for (Index y = 1; y < height - 1; ++y) {
                    for (Index x = 1; x < width - 1; ++x) {
                        const auto index = x + width * y;
                        result.work_ += 6 + round;
                        if (eligibility.cells()[static_cast<std::size_t>(index)] != EligibilityCell::eligible) { continue; }
                        const auto center = heights[static_cast<std::size_t>(index)];
                        const auto slope = std::max(
                            std::max(difference(center, heights[static_cast<std::size_t>(index - 1)]),
                                     difference(center, heights[static_cast<std::size_t>(index + 1)])),
                            std::max(difference(center, heights[static_cast<std::size_t>(index - width)]),
                                     difference(center, heights[static_cast<std::size_t>(index + width)])));
                        bool eligible = center > options.sea_level && slope <= options.maximum_slope;
                        for (Index slot = 0; slot < round; ++slot) {
                            if (slot < result.selection_.achieved_) {
                                const auto& prior = result.selection_.sites_[static_cast<std::size_t>(slot)];
                                if (distance(x, prior.x) + distance(y, prior.y) < options.minimum_spacing) { eligible = false; }
                            }
                        }
                        if (eligible && (best.index == UINT64_MAX || slope < best.slope)) { best = {index, x, y, slope, center}; }
                    }
                }
            }
            ++result.work_;
            if (best.index != UINT64_MAX) { result.selection_.sites_[static_cast<std::size_t>(result.selection_.achieved_++)] = best; }
        }
        return result;
    }
    SettlementResult<EligibleSettlementSites> select_eligible_settlement_sites(
        const Elevation& elevation, const EligibilityImage& eligibility, SettlementOptions options, CaveBudgets budgets) {
        return select_eligible_settlement_sites(elevation.descriptor().domain, elevation.heights(), eligibility, options, budgets);
    }
}
