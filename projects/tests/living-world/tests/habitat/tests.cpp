#include "fixtures.h"

#include <iostream>
#include <type_traits>

namespace {
    using namespace CE::LivingWorld;
    using namespace Habitat;
    using namespace Habitat::Fixture;
    static_assert(!std::is_copy_constructible_v<Eligibility>);
    static_assert(!std::is_move_constructible_v<Eligibility>);
    static_assert(!std::is_copy_assignable_v<Eligibility>);
    static_assert(!std::is_move_assignable_v<Eligibility>);
    constexpr ProcGen::SettlementOptions options{2, 0, 1, 2};
    void literal_case() {
        const auto setup = literal();
        auto advice = advisor(setup);
        const auto plan = take(advice->prepare(policy(), options));
        const auto current = take(advice->read(plan));
        check(current.places().size() == 1 && current.places()[0].terrain.index == 7, "middle place found");
        check(current.places()[0].key == Sites::SiteKey{{2, 1}, 2, 1, 7}, "stable place key");
        check(current.resident_slots() == 4 && current.activity_slots() == 2, "declared host slots not carrying capacity");
        check(current.places()[0].opportunity.view() == "host-garden-opportunity", "owned opportunity metadata");
        check(current.selection().shortfall() == 1 && current.selection().coverage_complete(), "honest greedy shortfall");
        check(!current.evidence().replay() && current.evidence().literal_schema() == 1, "literal provenance not generated");
        check(current.evidence().seed() == seed() && current.stamp().frame.origin_x == -3, "seed and actual frame retained");
        check(take(setup.owner->preserved(setup.owner->access())) == setup.preserved, "proposals never inhabit or mutate");
    }
    void generated_case() {
        const auto setup = generated();
        auto advice = advisor(setup);
        const auto plan = take(advice->prepare(policy(), {3, 0, 0, 1}));
        check(plan.places().size() == 1 && plan.places()[0].terrain.index == 7, "real Elevation site");
        check(plan.places()[0].terrain.height == 10 && plan.places()[0].terrain.slope == 0, "independent flat generator expectation");
        check(plan.evidence().replay().has_value() && plan.evidence().literal_schema() == 0, "real replay provenance");
        check(plan.evidence().replay()->random == ProcGen::StreamKey{42, 7, 1}, "actual generator stream preserved");
        check(!plan.selection().coverage_complete() && plan.selection().unknown_excluded() == 5,
            "only explicit sample7 eligible; five interior gaps not inferred dry");
        check(plan.selection().shortfall() == 2 && plan.resident_slots() == 4, "requested places not fabricated population");
    }
    void invalid_case() {
        const auto setup = literal();
        const auto access = setup.owner->access();
        const auto source = take(Eligibility::capture(setup.owner, access));
        auto advice = advisor(setup);
        auto invalid_policy = policy();
        invalid_policy.residential.unit = Unit::activity_slots;
        refused(advice->prepare(invalid_policy, options), Error::capacity);
        invalid_policy = policy();
        invalid_policy.residential.slots = UINT64_MAX;
        refused(advice->prepare(invalid_policy, options), Error::capacity);
        invalid_policy = policy();
        invalid_policy.version = 2;
        refused(advice->prepare(invalid_policy, options), Error::capacity);
        invalid_policy = policy();
        invalid_policy.capacity_version = 2;
        refused(advice->prepare(invalid_policy, options), Error::capacity);
        invalid_policy = policy();
        invalid_policy.opportunity = {};
        refused(advice->prepare(invalid_policy, options), Error::capacity);
        auto invalid_seed = seed();
        invalid_seed.version = 2;
        refused(Eligibility::literal(source, access, 1, setup.evidence->image(), 2, invalid_seed), Error::evidence);
        invalid_seed = seed();
        invalid_seed.name_space = {};
        refused(Eligibility::literal(source, access, 1, setup.evidence->image(), 2, invalid_seed), Error::evidence);
        refused(Eligibility::literal(source, access, 1, setup.evidence->image(), 0, seed()), Error::evidence);
        refused(Eligibility::literal(source, access, 2, setup.evidence->image(), 2, seed()), Error::evidence);
        const auto other_domain = take(ProcGen::Domain::checked(std::array<Index, 2>{3, 5}));
        std::array<ProcGen::EligibilityCell, 15> cells;
        cells.fill(ProcGen::EligibilityCell::ineligible);
        const auto wrong_image = take(ProcGen::EligibilityImage::make(other_domain, {1, 1, ProcGen::EligibilityCoverage::complete},
            cells, ProcGen::eligibility_hard_limits));
        refused(Eligibility::literal(source, access, 1, wrong_image, 2, seed()), Error::evidence);
        const auto generated_setup = generated();
        const auto generated_source = take(Eligibility::capture(generated_setup.owner, generated_setup.owner->access()));
        refused(Eligibility::literal(generated_source, generated_setup.owner->access(), 1, generated_setup.evidence->image(), 2, seed()), Error::evidence);
        const auto wrong_elevation = take(ProcGen::generate_elevation(std::array<Index, 2>{5, 3}, {42, 7, 1},
            {3, 10, 10}, ProcGen::elevation_hard_limits));
        refused(Eligibility::generated(source, access, wrong_elevation, setup.evidence->image(), 2, seed()), Error::evidence);
        auto bad_frame = setup.evidence->stamp();
        bad_frame.frame.width = 3;
        const auto original = take(setup.owner->capture(access));
        const auto water = Sites::WaterFacts::make(bad_frame, original.water.bindings(), original.water.facts());
        check(!water && water.error().reason == Sites::Error::invalid_frame, "real owner rejects malformed frame before habitat");
        check(take(setup.owner->preserved(access)) == setup.preserved, "all invalid proposals preserve world");
    }
    void budgets_case() {
        const auto setup = literal();
        const auto source = take(Eligibility::capture(setup.owner, setup.owner->access()));
        auto advice = advisor(setup);
        const auto needed = take(Advisor::requirements(setup.evidence->image().domain(), options));
        const Sites::Budget exact{needed.storage_bytes, needed.work};
        const auto plan = take(advice->prepare(policy(), options, ProcGen::eligibility_hard_limits, exact));
        check(plan.accounting().storage_bytes == needed.storage_bytes && plan.accounting().work == needed.work, "declared accounting admitted exactly");
        static_cast<void>(take(advice->read(plan, exact)));
        refused(advice->prepare(policy(), options, ProcGen::eligibility_hard_limits, {needed.storage_bytes - 1, needed.work}), Error::budget);
        refused(advice->prepare(policy(), options, ProcGen::eligibility_hard_limits, {needed.storage_bytes, needed.work - 1}), Error::budget);
        refused(advice->read(plan, {0, 0}), Error::budget);
        refused(Advisor::create(setup.owner, setup.owner->access(), setup.evidence, {0, 0}), Error::budget);
        refused(Eligibility::capture(setup.owner, setup.owner->access(), {0, 0}), Error::budget);
        refused(Eligibility::literal(source, setup.owner->access(), 1, setup.evidence->image(), 2, seed(), {0, 0}), Error::budget);
        const auto evidence_needed = Eligibility::requirements(15);
        const auto evidence = take(Eligibility::literal(source, setup.owner->access(), 1, setup.evidence->image(), 2,
            seed(), {evidence_needed.storage_bytes, evidence_needed.work}));
        check(evidence->revision() == 2, "exact evidence envelope accepted");
        refused(Eligibility::literal(source, setup.owner->access(), 1, setup.evidence->image(), 2,
            seed(), {evidence_needed.storage_bytes - 1, evidence_needed.work}), Error::budget);
        refused(Eligibility::literal(source, setup.owner->access(), 1, setup.evidence->image(), 2,
            seed(), {evidence_needed.storage_bytes, evidence_needed.work - 1}), Error::budget);
        const auto larger = generated();
        const auto larger_source = take(Eligibility::capture(larger.owner, larger.owner->access()));
        refused(Eligibility::literal(larger_source, larger.owner->access(), 1, setup.evidence->image(), 2,
            seed(), {evidence_needed.storage_bytes, evidence_needed.work}), Error::budget);
        const auto snapshot_needed = take(Advisor::requirements(setup.evidence->image().domain(), {0, 0, UINT32_MAX, 1}));
        auto exact_advice = take(Advisor::create(setup.owner, setup.owner->access(), setup.evidence,
            {snapshot_needed.storage_bytes, snapshot_needed.work}));
        check(exact_advice->replace_eligibility(evidence, {snapshot_needed.storage_bytes, snapshot_needed.work}).has_value(),
            "exact create and replacement envelope accepted");
        refused(advice->replace_eligibility(evidence, {snapshot_needed.storage_bytes - 1, snapshot_needed.work}), Error::budget);
        static_cast<void>(take(advice->read(plan)));
        const auto producer_refusal = advice->prepare(policy(), options, {0, 0, 0});
        refused(producer_refusal, Error::producer);
        check(producer_refusal.error().producer->reason == ProcGen::SettlementReason::cells, "producer failure retained");
        const auto empty = take(advice->prepare(policy(), {0, 0, 0, 1}));
        check(empty.places().empty() && empty.resident_slots() == 0 && empty.selection().work() == 15, "zero request not impossibility");
        check(take(setup.owner->preserved(setup.owner->access())) == setup.preserved, "budget refusals preserve world");
    }
    void water_case() {
        const auto setup = literal();
        const auto access = setup.owner->access();
        const auto source = take(Eligibility::capture(setup.owner, access));
        auto advice = advisor(setup);
        const auto plan = take(advice->prepare(policy(), options));
        const auto candidate = water_change(setup);
        check(take(setup.owner->adopt(access, candidate)).receipt.water_revision == 5, "real water adoption");
        refused(Eligibility::literal(source, access, 1, setup.evidence->image(), 2, seed()), Error::stale);
        refused(advice->read(plan), Error::stale);
        refused(advice->prepare(policy(), options), Error::stale);
        refused(Advisor::create(setup.owner, access, setup.evidence), Error::stale);
        check(plan.stamp().water == 4 && plan.places()[0].terrain.index == 7, "historical owned plan survives");
        check(take(setup.owner->preserved(access)) == setup.preserved, "water and habitat preserve town edits debts knowledge");
    }
    void excavation_case() {
        const auto setup = literal();
        const auto access = setup.owner->access();
        const auto source = take(Eligibility::capture(setup.owner, access));
        auto advice = advisor(setup);
        const auto plan = take(advice->prepare(policy(), options));
        const Sites::Edit edit{13, 62, true, text("player-action-62")};
        check(take(setup.owner->excavate(access, 19, edit)) == 20, "actual excavation before read");
        refused(Eligibility::literal(source, access, 1, setup.evidence->image(), 2, seed()), Error::stale);
        auto expected = setup.preserved;
        expected.edits[1] = edit;
        expected.edit_count = 2;
        refused(advice->read(plan), Error::stale);
        refused(advice->prepare(policy(), options), Error::stale);
        refused(Advisor::create(setup.owner, access, setup.evidence), Error::stale);
        const auto fresh = literal_evidence(setup, 2);
        check(advice->replace_eligibility(fresh).has_value(), "explicit refreshed edit-bound evidence");
        const auto refreshed = take(advice->prepare(policy(), options));
        check(refreshed.edits_revision() == 20 && refreshed.places()[0].key == plan.places()[0].key, "fresh proposal stable place key");
        refused(advice->read(plan), Error::stale);
        check(take(setup.owner->preserved(access)) == expected, "no relocation or lost accepted edit/debt/knowledge");
    }
    void fences_case() {
        const auto setup = literal();
        const auto foreign = literal();
        const auto access = setup.owner->access();
        const auto source = take(Eligibility::capture(setup.owner, access));
        auto advice = advisor(setup);
        auto other = advisor(foreign);
        const auto plan = take(advice->prepare(policy(), options));
        refused(other->read(plan), Error::foreign);
        refused(Advisor::create(foreign.owner, foreign.owner->access(), setup.evidence), Error::foreign);
        const auto wrong_access = Advisor::create(setup.owner, foreign.owner->access(), setup.evidence);
        refused(wrong_access, Error::owner);
        check(wrong_access.error().owner->reason == Sites::Error::denied, "same numeric fence not owner identity");
        refused(Eligibility::literal(source, foreign.owner->access(), 1, setup.evidence->image(), 2, seed()), Error::owner);
        check(setup.owner->renew_fence(access, {9, 3}).has_value(), "real fence renewal");
        refused(Eligibility::literal(source, setup.owner->access(), 1, setup.evidence->image(), 2, seed()), Error::stale);
        refused(advice->read(plan), Error::owner);
        const auto next = literal_evidence(setup, 2);
        auto renewed = take(Advisor::create(setup.owner, setup.owner->access(), next));
        refused(renewed->read(plan), Error::foreign);
        static_cast<void>(take(renewed->prepare(policy(), options)));
        check(take(setup.owner->preserved(setup.owner->access())) == setup.preserved, "fence operations do not rewrite world");
    }
    void revisions_case() {
        const auto setup = literal();
        const auto original_evidence = setup.evidence;
        auto advice = advisor(setup);
        const auto plan = take(advice->prepare(policy(), options));
        check(&plan.evidence() == original_evidence.get() && plan.evidence().revision() == 1,
            "original plan retains factory evidence");
        check(plan.selection().coverage_complete()
            && plan.evidence().image().policy().coverage == ProcGen::EligibilityCoverage::complete,
            "original plan and evidence start complete");
        const auto changed = literal_evidence(setup, 2, ProcGen::EligibilityCoverage::incomplete);
        check(advice->replace_eligibility(changed).has_value(), "owned evidence revision accepted");
        refused(advice->read(plan), Error::stale);
        check(&plan.evidence() == original_evidence.get() && plan.evidence().revision() == 1
            && original_evidence->revision() == 1, "replacement preserves historical evidence identity and revision");
        check(plan.selection().coverage_complete() && plan.selection().unknown_excluded() == 0
            && original_evidence->image().policy().coverage == ProcGen::EligibilityCoverage::complete,
            "replacement preserves historical selection and evidence coverage");
        const auto fresh = take(advice->prepare(policy(), options));
        check(&fresh.evidence() == changed.get() && fresh.evidence().revision() == 2
            && fresh.evidence().image().policy().coverage == ProcGen::EligibilityCoverage::incomplete,
            "fresh plan uses replacement factory evidence");
        check(!fresh.selection().coverage_complete() && fresh.selection().unknown_excluded() == 1, "changed coverage reported");
        refused(advice->replace_eligibility(changed), Error::revision);
        refused(advice->replace_eligibility(setup.evidence), Error::revision);
        const auto foreign = literal();
        const auto foreign_next = literal_evidence(foreign, 3);
        refused(advice->replace_eligibility(foreign_next), Error::foreign);
        const auto retained = take(advice->read(fresh));
        check(&retained.evidence() == changed.get() && retained.evidence().revision() == 2
            && retained.evidence().image().policy().coverage == ProcGen::EligibilityCoverage::incomplete,
            "foreign replacement preserves current accepted evidence");
        const auto after_refusal = take(advice->prepare(policy(), options));
        check(&after_refusal.evidence() == changed.get() && !after_refusal.selection().coverage_complete()
            && after_refusal.selection().unknown_excluded() == 1, "foreign refusal leaves replacement selected");
        check(take(setup.owner->preserved(setup.owner->access())) == setup.preserved, "local revision changes only derived advice");
    }
    void lifetime_case() {
        std::weak_ptr<Sites::Owner> retained_owner;
        const auto plan = [&retained_owner] {
            auto setup = generated();
            retained_owner = setup.owner;
            auto advice = advisor(setup);
            auto result = take(advice->prepare(policy(), {3, 0, 0, 1}));
            setup.owner.reset();
            setup.evidence.reset();
            check(!retained_owner.expired(), "real owner retained by advice/evidence lifetime");
            static_cast<void>(take(advice->read(result)));
            return result;
        }();
        check(plan.places()[0].terrain.index == 7 && plan.stamp().baseline->heights()[7] == 10, "output after all builders die");
        check(plan.evidence().replay().has_value() && plan.resident_slots() == 4, "owned replay and capacity after temporary destruction");
    }
}
int main(int argc, char** argv) {
    try {
        check(argc == 2, "one habitat case required");
        const std::string_view name = argv[1];
        if (name == "literal") { literal_case(); }
        else if (name == "generated") { generated_case(); }
        else if (name == "invalid") { invalid_case(); }
        else if (name == "budgets") { budgets_case(); }
        else if (name == "water") { water_case(); }
        else if (name == "excavation") { excavation_case(); }
        else if (name == "fences") { fences_case(); }
        else if (name == "revisions") { revisions_case(); }
        else if (name == "lifetime") { lifetime_case(); }
        else { throw std::runtime_error("unknown habitat case"); }
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "habitat failure: " << failure.what() << '\n';
        return 1;
    }
}
