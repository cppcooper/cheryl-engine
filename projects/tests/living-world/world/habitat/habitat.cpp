#include "habitat.h"

#include <algorithm>

namespace CE::LivingWorld::Habitat {
    class AdviceIdentity {};
    namespace {
        constexpr ProcGen::SettlementOptions snapshot_options{0, 0, UINT32_MAX, 1};
        Failure fail(Error reason) { return {reason, std::nullopt, std::nullopt}; }
        bool admitted(Sites::Accounting needed, Sites::Budget budget) {
            return needed.storage_bytes <= std::min(budget.storage_bytes, hard_limits.storage_bytes)
                && needed.work <= std::min(budget.work, hard_limits.work);
        }
        bool matches(const Sites::Image& read, const Eligibility& evidence) {
            return read.edits_revision == evidence.read_set().edits_revision
                && read.fence == evidence.read_set().fence && read.water.equivalent(evidence.read_set().water);
        }
    }
    HabitatPlan::HabitatPlan(Sites::Image read, std::shared_ptr<const Eligibility> evidence,
        std::shared_ptr<const AdviceIdentity> advisor, ProcGen::EligibleSettlementSites selected,
        Policy policy, Sites::Accounting accounting)
    : read_(std::move(read)), evidence_(std::move(evidence)), advisor_(std::move(advisor)),
      selected_(std::move(selected)), policy_(policy), accounting_(accounting) {
        std::size_t position = 0;
        for (const auto& site : selected_.sites()) {
            const Sites::SiteKey key{stamp().scope, stamp().baseline->identity(), stamp().frame.revision, site.index};
            places_[position++] = {key, site, policy.residential, policy.activity, policy.opportunity};
        }
    }
    Advisor::Advisor(std::shared_ptr<Sites::Owner> owner, Sites::Owner::Access access,
        std::shared_ptr<const Eligibility> evidence, std::shared_ptr<const AdviceIdentity> identity)
    : owner_(std::move(owner)), access_(std::move(access)), evidence_(std::move(evidence)), identity_(std::move(identity)) {}

    Result<Sites::Accounting> Advisor::requirements(const ProcGen::Domain& domain, ProcGen::SettlementOptions options) {
        const auto selector = ProcGen::eligible_settlement_requirements(domain, options);
        if (!selector) { return std::unexpected(Failure{Error::producer, selector.error(), std::nullopt}); }
        if (domain.cells() > ProcGen::eligibility_hard_limits.cells) { return std::unexpected(fail(Error::budget)); }
        const auto evidence = Eligibility::requirements(domain.cells());
        return Sites::Accounting{sizeof(Advisor) + sizeof(AdviceIdentity) + 2 * sizeof(Sites::Image)
                    + 2 * sizeof(HabitatPlan) + evidence.storage_bytes + selector->storage_bytes,
                evidence.work + selector->work + 4 * domain.cells() + 4 * Sites::capacity * Sites::capacity
                    + 2 * sizeof(HabitatPlan)};
    }
    Result<std::unique_ptr<Advisor>> Advisor::create(std::shared_ptr<Sites::Owner> owner,
        Sites::Owner::Access access, std::shared_ptr<const Eligibility> evidence, Sites::Budget budget) {
        if (!owner || !evidence) { return std::unexpected(fail(Error::evidence)); }
        if (!evidence->belongs_to(owner)) { return std::unexpected(fail(Error::foreign)); }
        const auto needed = requirements(evidence->image().domain(), snapshot_options);
        if (!needed) { return std::unexpected(needed.error()); }
        if (!admitted(*needed, budget)) { return std::unexpected(fail(Error::budget)); }
        const auto captured = owner->capture(access);
        if (!captured) { return std::unexpected(Failure{Error::owner, std::nullopt, captured.error()}); }
        if (!matches(*captured, *evidence)) { return std::unexpected(fail(Error::stale)); }
        return std::unique_ptr<Advisor>(new Advisor(std::move(owner), std::move(access), std::move(evidence),
            std::make_shared<const AdviceIdentity>()));
    }
    Result<Sites::Image> Advisor::current(Sites::Budget budget) const {
        const auto needed = requirements(evidence_->image().domain(), snapshot_options);
        if (!needed) { return std::unexpected(needed.error()); }
        if (!admitted(*needed, budget)) { return std::unexpected(fail(Error::budget)); }
        const auto captured = owner_->capture(access_);
        if (!captured) { return std::unexpected(Failure{Error::owner, std::nullopt, captured.error()}); }
        if (!matches(*captured, *evidence_)) { return std::unexpected(fail(Error::stale)); }
        return *captured;
    }
    Result<HabitatPlan> Advisor::prepare(Policy policy, ProcGen::SettlementOptions options,
        ProcGen::CaveBudgets producer_budget, Sites::Budget budget) const {
        if (policy.version != 1 || policy.capacity_version != 1 || policy.residential.unit != Unit::resident_slots
            || policy.activity.unit != Unit::activity_slots || policy.residential.slots > 1'000'000
            || policy.activity.slots > 1'000'000 || policy.opportunity.view().empty()) {
            return std::unexpected(fail(Error::capacity));
        }
        const auto needed = requirements(evidence_->image().domain(), options);
        if (!needed) { return std::unexpected(needed.error()); }
        if (!admitted(*needed, budget)) { return std::unexpected(fail(Error::budget)); }
        const auto captured = current(budget);
        if (!captured) { return std::unexpected(captured.error()); }
        const auto selected = ProcGen::select_eligible_settlement_sites(captured->water.stamp().baseline->domain(),
            captured->water.stamp().baseline->heights(), evidence_->image(), options, producer_budget);
        if (!selected) { return std::unexpected(Failure{Error::producer, selected.error(), std::nullopt}); }
        return HabitatPlan(*captured, evidence_, identity_, *selected, policy, *needed);
    }
    Result<HabitatPlan> Advisor::read(const HabitatPlan& plan, Sites::Budget budget) const {
        if (plan.advisor_ != identity_) { return std::unexpected(fail(Error::foreign)); }
        const auto needed = requirements(evidence_->image().domain(), plan.selection().options());
        if (!needed) { return std::unexpected(needed.error()); }
        if (!admitted(*needed, budget)) { return std::unexpected(fail(Error::budget)); }
        const auto captured = current(budget);
        if (!captured) { return std::unexpected(captured.error()); }
        if (plan.evidence_ != evidence_ || plan.read_.fence != captured->fence
            || plan.read_.edits_revision != captured->edits_revision || !plan.read_.water.equivalent(captured->water)) {
            return std::unexpected(fail(Error::stale));
        }
        return plan;
    }
    Result<void> Advisor::replace_eligibility(std::shared_ptr<const Eligibility> evidence, Sites::Budget budget) {
        if (!evidence || evidence->revision() <= evidence_->revision()) { return std::unexpected(fail(Error::revision)); }
        if (!evidence->belongs_to(owner_)) { return std::unexpected(fail(Error::foreign)); }
        const auto needed = requirements(evidence->image().domain(), snapshot_options);
        if (!needed) { return std::unexpected(needed.error()); }
        if (!admitted(*needed, budget)) { return std::unexpected(fail(Error::budget)); }
        const auto captured = owner_->capture(access_);
        if (!captured) { return std::unexpected(Failure{Error::owner, std::nullopt, captured.error()}); }
        if (!matches(*captured, *evidence)) { return std::unexpected(fail(Error::stale)); }
        evidence_ = std::move(evidence);
        return {};
    }
}
