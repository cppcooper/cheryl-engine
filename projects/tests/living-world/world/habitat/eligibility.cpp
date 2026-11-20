#include "eligibility.h"

#include <algorithm>

namespace CE::LivingWorld::Habitat {
    namespace {
        Failure fail(Error reason) { return {reason, std::nullopt, std::nullopt}; }
        Result<Sites::Image> capture(const std::shared_ptr<Sites::Owner>& owner, const Sites::Owner::Access& access,
            const ProcGen::EligibilityImage& image, Sites::Budget budget) {
            if (!owner) { return std::unexpected(fail(Error::evidence)); }
            const auto needed = Eligibility::requirements(image.domain().cells());
            if (image.domain().cells() > ProcGen::eligibility_hard_limits.cells
                || needed.storage_bytes > std::min(budget.storage_bytes, hard_limits.storage_bytes)
                || needed.work > std::min(budget.work, hard_limits.work)) {
                return std::unexpected(fail(Error::budget));
            }
            const auto read = owner->capture(access);
            if (!read) { return std::unexpected(Failure{Error::owner, std::nullopt, read.error()}); }
            return *read;
        }
    }
    Eligibility::Eligibility(std::shared_ptr<Sites::Owner> owner, Sites::Image read, ProcGen::EligibilityImage image, Index revision,
        Index schema, Seed seed, std::optional<ProcGen::ElevationReplay> replay)
    : owner_(std::move(owner)), read_(std::move(read)), image_(std::move(image)), revision_(revision), literal_schema_(schema),
      seed_(seed), replay_(std::move(replay)) {}

    Sites::Accounting Eligibility::requirements(Index cells) {
        if (cells > ProcGen::eligibility_hard_limits.cells) { return {UINT64_MAX, UINT64_MAX}; }
        const auto baseline = Sites::Baseline::requirements(cells);
        return {sizeof(Eligibility) + 2 * sizeof(Sites::Image) + sizeof(ProcGen::EligibilityImage) + 2 * cells * sizeof(ProcGen::EligibilityCell)
                    + baseline.storage_bytes,
                baseline.work + 4 * cells + sizeof(Eligibility)};
    }
    Result<void> Eligibility::validate(const Sites::Stamp& stamp, const ProcGen::EligibilityImage& image,
        Index revision, Seed seed, Sites::Budget budget) {
        if (!stamp.baseline || stamp.baseline->domain().dimensions() != 2 || stamp.scope.world == 0
            || stamp.scope.branch == 0 || stamp.terrain == 0 || stamp.water == 0 || stamp.policy == 0
            || revision == 0 || seed.version != 1 || seed.name_space.view().empty()) {
            return std::unexpected(fail(Error::evidence));
        }
        const auto cells = stamp.baseline->domain().cells();
        const auto needed = requirements(cells);
        if (cells > ProcGen::eligibility_hard_limits.cells
            || needed.storage_bytes > std::min(budget.storage_bytes, hard_limits.storage_bytes)
            || needed.work > std::min(budget.work, hard_limits.work)) {
            return std::unexpected(fail(Error::budget));
        }
        const auto& extents = stamp.baseline->domain().extents();
        if (stamp.frame.revision == 0 || stamp.frame.width != extents[0] || stamp.frame.height != extents[1]
            || image.domain() != stamp.baseline->domain() || image.cells().size() != cells) {
            return std::unexpected(fail(Error::evidence));
        }
        return {};
    }
    Result<std::shared_ptr<const Eligibility>> Eligibility::literal(std::shared_ptr<Sites::Owner> owner,
        const Sites::Owner::Access& access,
        Index schema, const ProcGen::EligibilityImage& image, Index revision, Seed seed, Sites::Budget budget) {
        const auto read = capture(owner, access, image, budget);
        if (!read) { return std::unexpected(read.error()); }
        const auto& stamp = read->water.stamp();
        const auto valid = validate(stamp, image, revision, seed, budget);
        if (!valid) { return std::unexpected(valid.error()); }
        const auto literal = Sites::Baseline::literal(stamp.baseline->identity(), schema,
            stamp.baseline->domain(), stamp.baseline->heights(), hard_limits);
        if (!literal || !stamp.baseline->equivalent(**literal)) { return std::unexpected(fail(Error::evidence)); }
        return std::shared_ptr<const Eligibility>(new Eligibility(std::move(owner), *read, image, revision, schema, seed, std::nullopt));
    }
    Result<std::shared_ptr<const Eligibility>> Eligibility::generated(std::shared_ptr<Sites::Owner> owner,
        const Sites::Owner::Access& access,
        const ProcGen::Elevation& elevation, const ProcGen::EligibilityImage& image, Index revision, Seed seed, Sites::Budget budget) {
        const auto read = capture(owner, access, image, budget);
        if (!read) { return std::unexpected(read.error()); }
        const auto& stamp = read->water.stamp();
        const auto valid = validate(stamp, image, revision, seed, budget);
        if (!valid) { return std::unexpected(valid.error()); }
        if (!stamp.baseline->matches(elevation)) { return std::unexpected(fail(Error::evidence)); }
        return std::shared_ptr<const Eligibility>(new Eligibility(std::move(owner), *read, image, revision, 0, seed, elevation.descriptor()));
    }
}
