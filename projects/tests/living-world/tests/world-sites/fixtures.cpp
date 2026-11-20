#include "fixtures.h"

namespace CE::LivingWorld::Sites::Fixture {
    std::array<ReachFact, 3> facts(bool diverted) {
        return {{
            {101, diverted ? WaterUse::not_usable : WaterUse::usable,
             diverted ? Reason::supply_lost : Reason::supply_available, text(diverted ? "host-observation-A5" : "host-observation-A4")},
            {102, WaterUse::not_usable, Reason::no_supply, text("host-observation-B4")},
            {103, WaterUse::unknown, Reason::no_measurement, text("host-gap-C4")}
        }};
    }
    std::array<Binding, 3> bindings(const Stamp& stamp) {
        return {{
            {{stamp.scope, stamp.baseline->identity(), stamp.frame.revision, 6}, 201, 101},
            {{stamp.scope, stamp.baseline->identity(), stamp.frame.revision, 7}, 202, 102},
            {{stamp.scope, stamp.baseline->identity(), stamp.frame.revision, 8}, 203, 103}
        }};
    }
    Preserved preserved() {
        Preserved state;
        state.town_count = state.edit_count = state.debt_count = state.knowledge_count = 1;
        state.towns[0] = {1, 6, 11, true};
        state.edits[0] = {12, 61, true, text("player-action-61")};
        state.debts[0] = {1, 5, 3};
        state.knowledge[0] = {1, 8, text("heard-repair-plan-8")};
        return state;
    }
    Setup generated() {
        const std::array<Index, 2> extents{5, 4};
        const auto elevation = take(ProcGen::generate_elevation(extents, {42, 7, 1}, {3, 10, 10}, ProcGen::elevation_hard_limits));
        check(elevation.heights().size() == 20, "actual generated dimensions");
        Stamp stamp{{1, 1}, take(Baseline::generated(1, elevation)), {2, 5, 4, 0, 0}, 7, 4, 1};
        const auto water = take(WaterFacts::make(stamp, bindings(stamp), facts()));
        auto batch = take(select_and_assess(elevation, stamp, water, {3, 0, 0, 1}, ProcGen::settlement_hard_limits));
        return {water, std::move(batch), preserved()};
    }
    Setup literal() {
        const std::array<Index, 2> extents{5, 3};
        const auto domain = take(ProcGen::Domain::checked(extents));
        std::array<ProcGen::Height, 15> heights;
        heights.fill(2);
        heights[2] = 3;
        Stamp stamp{{2, 1}, take(Baseline::literal(2, 1, domain, heights)), {1, 5, 3, 0, 0}, 1, 4, 1};
        const std::array<ReachFact, 3> wet_middle{{
            {101, WaterUse::not_usable, Reason::no_supply, text("literal-dry-6")},
            {102, WaterUse::usable, Reason::supply_available, text("literal-wet-7")},
            {103, WaterUse::not_usable, Reason::no_supply, text("literal-dry-8")}
        }};
        auto water = take(WaterFacts::make(stamp, bindings(stamp), wet_middle));
        auto batch = take(select_and_assess(domain, heights, stamp, water, {2, 0, 1, 2}, ProcGen::settlement_hard_limits));
        return {std::move(water), std::move(batch), preserved()};
    }
    std::unique_ptr<Owner> owner(const Setup& setup) {
        return take(Owner::create(setup.water, setup.preserved, 19, {9, 2}));
    }
    Candidate change(const Image& image, Index sequence, bool diverted) {
        return take(Candidate::prepare(image, {image.water.stamp().scope, 1, sequence}, image.water.bindings(), facts(diverted)));
    }
    Batch reassess(const WaterFacts& water) {
        return take(select_and_assess(water.stamp(), water, {3, 0, 0, 1}, ProcGen::settlement_hard_limits));
    }
}
