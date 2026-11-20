#include "fixtures.h"

namespace CE::LivingWorld::Habitat::Fixture {
    Sites::Preserved preserved() {
        Sites::Preserved state;
        state.town_count = state.edit_count = state.debt_count = state.knowledge_count = 1;
        state.towns[0] = {1, 6, 11, true};
        state.edits[0] = {12, 61, true, text("player-action-61")};
        state.debts[0] = {1, 5, 3};
        state.knowledge[0] = {1, 8, text("heard-repair-plan-8")};
        return state;
    }
    namespace {
        Setup from_baseline(std::shared_ptr<const Sites::Baseline> baseline, Index width, Index height) {
            Sites::Stamp stamp{{2, 1}, std::move(baseline), {1, width, height, -3, 8}, 1, 4, 1};
            const std::array<Sites::Binding, 1> bindings{{{{stamp.scope, stamp.baseline->identity(), 1, 7}, 201, 101}}};
            const std::array<Sites::ReachFact, 1> facts{{{101, Sites::WaterUse::usable, Sites::Reason::supply_available,
                text("explicit-host-sample-cell7")}}};
            const auto water = take(Sites::WaterFacts::make(stamp, bindings, facts));
            const auto state = preserved();
            std::shared_ptr<Sites::Owner> owner = take(Sites::Owner::create(water, state, 19, {9, 2}));
            return {std::move(owner), {}, state};
        }
    }
    std::shared_ptr<const Eligibility> literal_evidence(const Setup& setup, Index revision, ProcGen::EligibilityCoverage coverage) {
        const auto read = take(setup.owner->capture(setup.owner->access()));
        std::array<ProcGen::EligibilityCell, 15> cells;
        cells.fill(ProcGen::EligibilityCell::ineligible);
        cells[7] = ProcGen::EligibilityCell::eligible;
        if (coverage == ProcGen::EligibilityCoverage::incomplete) { cells[6] = ProcGen::EligibilityCell::unknown; }
        const auto image = take(ProcGen::EligibilityImage::make(read.water.stamp().baseline->domain(), {1, 1, coverage},
            cells, ProcGen::eligibility_hard_limits));
        return take(Eligibility::literal(setup.owner, setup.owner->access(), 1, image, revision, seed()));
    }
    Setup literal() {
        const auto domain = take(ProcGen::Domain::checked(std::array<Index, 2>{5, 3}));
        std::array<ProcGen::Height, 15> heights;
        heights.fill(2);
        heights[2] = 3;
        auto setup = from_baseline(take(Sites::Baseline::literal(2, 1, domain, heights)), 5, 3);
        setup.evidence = literal_evidence(setup, 1);
        return setup;
    }
    Setup generated() {
        const auto elevation = take(ProcGen::generate_elevation(std::array<Index, 2>{5, 4}, {42, 7, 1},
            {3, 10, 10}, ProcGen::elevation_hard_limits));
        auto setup = from_baseline(take(Sites::Baseline::generated(3, elevation)), 5, 4);
        std::array<ProcGen::EligibilityCell, 20> cells;
        cells.fill(ProcGen::EligibilityCell::unknown);
        cells[7] = ProcGen::EligibilityCell::eligible;
        const auto image = take(ProcGen::EligibilityImage::make(elevation.descriptor().domain,
            {1, 1, ProcGen::EligibilityCoverage::incomplete}, cells, ProcGen::eligibility_hard_limits));
        setup.evidence = take(Eligibility::generated(setup.owner, setup.owner->access(), elevation, image, 1, seed()));
        return setup;
    }
    std::unique_ptr<Advisor> advisor(const Setup& setup) {
        return take(Advisor::create(setup.owner, setup.owner->access(), setup.evidence));
    }
    Sites::Candidate water_change(const Setup& setup) {
        const auto read = take(setup.owner->capture(setup.owner->access()));
        const std::array<Sites::ReachFact, 1> diverted{{{101, Sites::WaterUse::not_usable, Sites::Reason::supply_lost,
            text("host-water-change")}}};
        return take(Sites::Candidate::prepare(read, {read.water.stamp().scope, 1, 1}, read.water.bindings(), diverted));
    }
}
