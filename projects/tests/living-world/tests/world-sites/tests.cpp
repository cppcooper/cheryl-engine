#include "fixtures.h"

#include <algorithm>
#include <iostream>

namespace CE::LivingWorld::Sites::Fixture {
    namespace {
        template <class Value> void refused(const Result<Value>& result, Error reason) {
            check(!result && result.error().reason == reason && !result.error().producer, "exact adapter refusal");
        }
        void selector_refused(const Result<Batch>& result, ProcGen::FailureKind kind, ProcGen::SettlementReason reason) {
            check(!result && result.error().reason == Error::producer_failure
                && result.error().producer == ProcGen::SettlementFailure{kind, reason}, "exact original producer refusal");
        }
        void literal_case() {
            const auto setup = literal();
            const auto& batch = setup.batch;
            check(batch.requested() == 2 && batch.achieved() == 2 && batch.shortfall() == 0, "terrain request is fulfilled");
            check(batch.selector_work() == 35, "literal producer work is 35");
            check(batch.selector_storage() == sizeof(ProcGen::SettlementSites) + sizeof(ProcGen::SettlementSite), "actual selector accounting");
            check(batch.sites()[0].terrain == ProcGen::SettlementSite{6, 1, 1, 0, 2}, "literal first dry site");
            check(batch.sites()[1].terrain == ProcGen::SettlementSite{8, 3, 1, 0, 2}, "literal second dry site");
            check(batch.count(Verdict::unsuitable) == 2 && batch.count(Verdict::suitable) == 0, "only wet neighbor is absent");
            check(batch.outcome() == ShortlistOutcome::no_eligible_in_shortlist && !batch.water_search_complete(), "not global failure");
        }
        void generated_case() {
            const auto setup = generated();
            const auto& batch = setup.batch;
            check(batch.requested() == 3 && batch.achieved() == 3 && batch.shortfall() == 0, "actual generated selector counts");
            check(batch.selector_work() == 111, "actual generated selector work");
            for (std::size_t index = 0; index < 3; ++index) {
                const auto cell = static_cast<Index>(index) + 6;
                check(batch.sites()[index].terrain == ProcGen::SettlementSite{cell, cell - 5, 1, 0, 10}, "generated row-major sites");
            }
            check(batch.count(Verdict::suitable) == 1 && batch.count(Verdict::unsuitable) == 1
                && batch.count(Verdict::unknown) == 1, "generated mixed facts");
            check(batch.sites()[0].evidence.view() == "host-observation-A4", "owned evidence");
            const auto short_batch = take(select_and_assess(setup.water.stamp(), setup.water, {16, 0, 0, 1}, ProcGen::settlement_hard_limits));
            check(short_batch.achieved() == 6 && short_batch.shortfall() == 10, "successful producer exhaustion is not budget refusal");
            const auto empty = take(select_and_assess(setup.water.stamp(), setup.water, {0, 0, 0, 1}, ProcGen::settlement_hard_limits));
            check(empty.requested() == 0 && empty.achieved() == 0 && !empty.water_search_complete(), "empty successful batch");
        }
        void water_case() {
            const auto setup = generated();
            auto live = owner(setup);
            const auto access = live->access();
            const auto image = take(live->capture(access));
            const auto candidate = change(image, 21);
            const auto late = change(image, 22);
            check(take(live->read(access, setup.batch)).count(Verdict::suitable) == 1, "fresh old-water advice");
            check(!take(live->lookup(access, candidate)), "unknown lookup is not an applied receipt");
            const auto applied = take(live->adopt(access, candidate));
            check(!applied.existing && applied.receipt.water_revision == 5, "one water4 to5 publication");
            refused(live->read(access, setup.batch), Error::stale);
            const auto late_batch = reassess(image.water);
            refused(live->read(access, late_batch), Error::stale);
            refused(live->adopt(access, late), Error::stale);
            const auto current = take(live->capture(access));
            check(current.water.stamp().water == 5 && current.edits_revision == 19, "late work cannot roll back water");
            const auto fresh = reassess(current.water);
            check(take(live->read(access, fresh)).count(Verdict::unsuitable) == 2, "fresh advice is unsuitable not stale");
            check(take(live->preserved(access)) == setup.preserved, "town excavation debt and knowledge unchanged");
            check(setup.batch.sites()[0].verdict == Verdict::suitable && setup.batch.stamp().water == 4, "old diagnostic remains historical");
            check(take(live->lookup(access, candidate)) == applied.receipt, "lost reply resolves from retained receipt");
            check(take(live->adopt(access, candidate)).existing, "same body never reapplies");
            const auto changed_body = change(image, 21, false);
            refused(live->adopt(access, changed_body), Error::body_conflict);
            auto reordered_bindings = bindings(image.water.stamp());
            auto reordered_facts = facts(true);
            std::ranges::reverse(reordered_bindings);
            std::ranges::reverse(reordered_facts);
            const auto equivalent = take(Candidate::prepare(image, {image.water.stamp().scope, 1, 21}, reordered_bindings, reordered_facts));
            check(equivalent.same_body(candidate) && take(live->adopt(access, equivalent)).existing, "canonical exact structural body");
            check(take(live->capture(access)).water.equivalent(current.water), "all retries preserve water5");
        }
        void excavation_case() {
            const auto setup = generated();
            auto live = owner(setup);
            const auto access = live->access();
            const auto candidate = change(take(live->capture(access)), 21);
            check(take(live->excavate(access, 19, {13, 62, true, text("player-action-62")})) == 20, "accepted excavation advances edits");
            auto expected = setup.preserved;
            expected.edits[1] = {13, 62, true, text("player-action-62")};
            expected.edit_count = 2;
            refused(live->adopt(access, candidate), Error::edits_conflict);
            check(take(live->capture(access)).water.stamp().water == 4, "edits conflict has no partial water publication");
            check(take(live->preserved(access)) == expected, "both accepted edits survive refusal");
            const auto fresh = change(take(live->capture(access)), 22);
            check(take(live->adopt(access, fresh)).receipt.water_revision == 5, "reprepared candidate uses edits20");
            check(take(live->preserved(access)) == expected, "current preserved set survives adoption");
            refused(live->excavate(access, 19, {14, 63, true, text("old-action")}), Error::edits_conflict);
        }
        void invalid_case() {
            const auto setup = generated();
            const auto stamp = setup.water.stamp();
            auto binding_values = bindings(stamp);
            auto fact_values = facts();
            binding_values[1] = binding_values[0];
            refused(WaterFacts::make(stamp, binding_values, fact_values), Error::invalid_facts);
            binding_values = bindings(stamp);
            binding_values[2].intake = binding_values[0].intake;
            refused(WaterFacts::make(stamp, binding_values, fact_values), Error::invalid_facts);
            binding_values = bindings(stamp);
            ++binding_values[0].site.baseline;
            refused(WaterFacts::make(stamp, binding_values, fact_values), Error::invalid_facts);
            binding_values = bindings(stamp);
            binding_values[2].site.cell = 19;
            binding_values[2].site.scope.world = 99;
            refused(WaterFacts::make(stamp, binding_values, fact_values), Error::invalid_facts);
            fact_values[2] = fact_values[0];
            refused(WaterFacts::make(stamp, bindings(stamp), fact_values), Error::invalid_facts);
            fact_values = facts();
            fact_values[2].use = static_cast<WaterUse>(99);
            refused(WaterFacts::make(stamp, bindings(stamp), fact_values), Error::invalid_facts);
            refused(Text::from(std::string(65, 'x')), Error::invalid_facts);
            refused(Text::from("\x01"), Error::invalid_facts);
            auto bad = stamp;
            bad.frame.width = 4;
            refused(WaterFacts::make(bad, bindings(bad), facts()), Error::invalid_frame);
            bad = stamp;
            bad.baseline.reset();
            refused(WaterFacts::make(bad, {}, {}), Error::invalid_baseline);
            bad = stamp;
            bad.water = 5;
            refused(select_and_assess(bad, setup.water, {3, 0, 0, 1}, ProcGen::settlement_hard_limits), Error::invalid_facts);
            const std::array<Index, 2> extents{5, 4};
            const auto elevation = take(ProcGen::generate_elevation(extents, {42, 7, 1}, {3, 11, 11}, ProcGen::elevation_hard_limits));
            refused(select_and_assess(elevation, stamp, setup.water, {3, 0, 0, 1}, ProcGen::settlement_hard_limits), Error::invalid_baseline);
            const auto unknown_water = take(WaterFacts::make(stamp, {}, {}));
            check(reassess(unknown_water).count(Verdict::unknown) == 3, "missing mappings are unknown");
            binding_values = bindings(stamp);
            binding_values[0].reach = 0;
            fact_values = facts();
            fact_values[1].evidence = Text{};
            const auto gaps = take(WaterFacts::make(stamp, binding_values, fact_values));
            const auto gap_batch = reassess(gaps);
            check(gap_batch.count(Verdict::unknown) == 3, "unmapped and unavailable evidence are unknown");
            check(gap_batch.sites()[0].reason == Reason::unmapped && gap_batch.sites()[1].reason == Reason::missing_evidence, "explicit unknown reasons");
            const auto missing_fact = take(WaterFacts::make(stamp, bindings(stamp), {}));
            check(reassess(missing_fact).sites()[0].reason == Reason::missing_fact, "missing reach is unknown");
            auto live = owner(setup);
            refused(live->read(live->access(), reassess(unknown_water)), Error::invalid_facts);
            auto conflicting = take(live->capture(live->access()));
            conflicting.water = unknown_water;
            const auto wrong_basis = change(conflicting, 21);
            refused(live->adopt(live->access(), wrong_basis), Error::invalid_facts);
            check(take(live->capture(live->access())).water.equivalent(setup.water), "same-version conflicting facts cannot publish");
        }
        void budgets_case() {
            const auto setup = generated();
            const auto& stamp = setup.water.stamp();
            selector_refused(select_and_assess(stamp, setup.water, {17, 0, 0, 1}, ProcGen::settlement_hard_limits),
                ProcGen::FailureKind::invalid_input, ProcGen::SettlementReason::options);
            selector_refused(select_and_assess(stamp, setup.water, {3, 0, 0, 0}, ProcGen::settlement_hard_limits),
                ProcGen::FailureKind::invalid_input, ProcGen::SettlementReason::options);
            selector_refused(select_and_assess(stamp.baseline->domain(), stamp.baseline->heights().first(19), stamp, setup.water,
                {3, 0, 0, 1}, ProcGen::settlement_hard_limits), ProcGen::FailureKind::invalid_input, ProcGen::SettlementReason::height_count);
            auto producer_budget = ProcGen::settlement_hard_limits;
            producer_budget.work = 110;
            selector_refused(select_and_assess(stamp, setup.water, {3, 0, 0, 1}, producer_budget),
                ProcGen::FailureKind::budget_refused, ProcGen::SettlementReason::work);
            producer_budget = ProcGen::settlement_hard_limits;
            producer_budget.cells = 19;
            selector_refused(select_and_assess(stamp, setup.water, {3, 0, 0, 1}, producer_budget),
                ProcGen::FailureKind::budget_refused, ProcGen::SettlementReason::cells);
            producer_budget = ProcGen::settlement_hard_limits;
            producer_budget.storage_bytes = 0;
            selector_refused(select_and_assess(stamp, setup.water, {3, 0, 0, 1}, producer_budget),
                ProcGen::FailureKind::budget_refused, ProcGen::SettlementReason::storage);
            const auto needed = Batch::requirements(20);
            refused(select_and_assess(stamp, setup.water, {3, 0, 0, 1}, ProcGen::settlement_hard_limits,
                {needed.storage_bytes - 1, needed.work}), Error::budget);
            refused(select_and_assess(stamp, setup.water, {3, 0, 0, 1}, ProcGen::settlement_hard_limits,
                {needed.storage_bytes, needed.work - 1}), Error::budget);
            check(take(select_and_assess(stamp, setup.water, {3, 0, 0, 1}, ProcGen::settlement_hard_limits,
                {needed.storage_bytes, needed.work})).achieved() == 3, "exact adapter accounting admitted");
            refused(WaterFacts::make(stamp, bindings(stamp), facts(), {0, 0}), Error::budget);
            refused(Baseline::literal(2, 1, stamp.baseline->domain(), stamp.baseline->heights(), {0, 0}), Error::budget);
            refused(Owner::create(setup.water, setup.preserved, 19, {9, 2}, {0, 0}), Error::budget);
            auto live = owner(setup);
            const auto access = live->access();
            const auto image = take(live->capture(access));
            refused(Candidate::prepare(image, {stamp.scope, 1, 21}, bindings(stamp), facts(true), {0, 0}), Error::budget);
            const auto candidate = change(image, 21);
            refused(live->adopt(access, candidate, {0, 0}), Error::budget);
            check(take(live->capture(access)).water.equivalent(image.water) && take(live->preserved(access)) == setup.preserved, "budget refusal is atomic");
        }
        void lifetime_case() {
            const auto setup = generated();
            check(setup.batch.sites()[0].terrain.height == 10 && setup.batch.sites()[0].evidence.view() == "host-observation-A4", "producer input destroyed before diagnostics");
            auto live = owner(setup);
            const auto candidate = [&live] {
                const auto short_image = take(live->capture(live->access()));
                return change(short_image, 21);
            }();
            check(take(live->adopt(live->access(), candidate)).receipt.water_revision == 5, "candidate owns destroyed image and facts");
            auto replacement_owner = take(Owner::create(setup.water, setup.preserved, 19, {10, 2}));
            refused(replacement_owner->adopt(replacement_owner->access(), candidate), Error::fence);
            auto same_numbers = owner(setup);
            refused(same_numbers->adopt(same_numbers->access(), candidate), Error::fence);
            refused(same_numbers->capture(live->access()), Error::denied);
            const auto other_baseline = generated();
            check(other_baseline.water.stamp().baseline != setup.water.stamp().baseline
                && other_baseline.water.stamp().equivalent(setup.water.stamp()), "content equality is not pointer identity");
            const auto other_frame = [&] {
                auto changed = setup.water.stamp();
                ++changed.frame.revision;
                return take(WaterFacts::make(changed, bindings(changed), facts()));
            }();
            auto reframed = take(Owner::create(other_frame, setup.preserved, 19, {10, 1}));
            refused(reframed->read(reframed->access(), setup.batch), Error::stale);
            auto policy = setup.water.stamp();
            ++policy.policy;
            auto policy_owner = take(Owner::create(take(WaterFacts::make(policy, bindings(policy), facts())), setup.preserved, 19, {11, 1}));
            refused(policy_owner->read(policy_owner->access(), setup.batch), Error::stale);
        }
        void fences_case() {
            const auto setup = generated();
            auto live = owner(setup);
            auto foreign = owner(setup);
            const auto access = live->access();
            const auto image = take(live->capture(access));
            const auto candidate = change(image, 21);
            const auto late = change(image, 22);
            refused(live->adopt(foreign->access(), candidate), Error::denied);
            refused(live->read(foreign->access(), setup.batch), Error::denied);
            static_cast<void>(take(live->adopt(access, candidate)));
            check(live->renew_fence(access, {9, 3}).has_value(), "owner advances epoch");
            refused(live->capture(access), Error::denied);
            const auto fresh_access = live->access();
            refused(live->adopt(fresh_access, late), Error::fence);
            check(take(live->adopt(fresh_access, candidate)).existing, "authorized lookup precedes old preconditions");
            const auto other_scope = literal();
            refused(live->read(fresh_access, other_scope.batch), Error::denied);
        }
        void retention_case() {
            const auto setup = generated();
            auto live = owner(setup);
            const auto access = live->access();
            const auto first = change(take(live->capture(access)), 1);
            static_cast<void>(take(live->adopt(access, first)));
            for (Index index = 2; index <= receipt_capacity; ++index) {
                const auto next = change(take(live->capture(access)), index);
                check(!take(live->adopt(access, next)).existing, "bounded new receipt slot");
            }
            const auto before = take(live->capture(access));
            const auto overflow = change(before, 9);
            refused(live->adopt(access, overflow), Error::receipt_full);
            check(take(live->adopt(access, first)).existing, "full table still answers old applied lookup");
            check(take(live->capture(access)).water.equivalent(before.water), "capacity refusal publishes nothing");
            check(take(live->preserved(access)) == setup.preserved, "retention has no unrelated effects");
        }
    }
    void run(std::string_view name) {
        if (name == "literal") { literal_case(); }
        else if (name == "generated") { generated_case(); }
        else if (name == "water") { water_case(); }
        else if (name == "excavation") { excavation_case(); }
        else if (name == "invalid") { invalid_case(); }
        else if (name == "budgets") { budgets_case(); }
        else if (name == "lifetime") { lifetime_case(); }
        else if (name == "fences") { fences_case(); }
        else if (name == "retention") { retention_case(); }
        else { throw std::runtime_error("unknown sites case"); }
    }
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) { throw std::runtime_error("expected one case name"); }
        CE::LivingWorld::Sites::Fixture::run(argv[1]);
        std::cout << "sites " << argv[1] << " passes\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "sites failure: " << failure.what() << '\n';
        return 1;
    }
}
