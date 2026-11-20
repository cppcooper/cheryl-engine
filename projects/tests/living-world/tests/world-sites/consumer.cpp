#include "fixtures.h"

#include <cheryl/maps/tilemap.h>

#include <iostream>

#if defined(CE_ENGINE_STATIC) || defined(CE_BUILDING_ENGINE)
#error Sites consumer requires the normal shared Engine import contract.
#endif

int main() {
    namespace Sites = CE::LivingWorld::Sites;
    namespace Fixture = Sites::Fixture;
    try {
        const auto literal = Fixture::literal();
        Fixture::check(literal.batch.outcome() == Sites::ShortlistOutcome::no_eligible_in_shortlist, "literal dry shortlist");
        std::cout << "literal: NoEligibleInShortlist; requested=" << literal.batch.requested()
                  << " achieved=" << literal.batch.achieved() << " producer-shortfall=" << literal.batch.shortfall()
                  << " water-search-complete=false\n";
        const auto setup = Fixture::generated();
        auto owner = Fixture::owner(setup);
        const auto access = owner->access();
        CE::Tilemaps::Map presentation({5, 4}, 2);
        presentation.set({0}, {1, 1}, {100});
        const auto display_advice = [&presentation](const Sites::Batch& batch) {
            for (const auto& site : batch.sites()) {
                const std::uint32_t marker = site.verdict == Sites::Verdict::suitable ? 1U
                    : site.verdict == Sites::Verdict::unsuitable ? 2U : 3U;
                presentation.set({1}, {static_cast<std::int64_t>(site.terrain.x), static_cast<std::int64_t>(site.terrain.y)}, {marker});
            }
        };
        const auto print = [](const Sites::Batch& batch) {
            for (const auto& site : batch.sites()) {
                const auto verdict = site.verdict == Sites::Verdict::suitable ? "Suitable"
                    : site.verdict == Sites::Verdict::unsuitable ? "Unsuitable" : "Unknown";
                std::cout << "water=" << batch.stamp().water << " site=" << site.terrain.index
                          << " (" << site.terrain.x << ',' << site.terrain.y << ") slope=" << site.terrain.slope
                          << ' ' << verdict << " evidence=" << site.evidence.view() << '\n';
            }
        };
        const auto initial = Fixture::take(owner->read(access, setup.batch));
        print(initial);
        display_advice(initial);
        Fixture::check(presentation.get({1}, {1, 1}).value == 1, "real Engine map shows initial suitable advice");
        const auto candidate = Fixture::change(Fixture::take(owner->capture(access)), 21);
        const auto applied = Fixture::take(owner->adopt(access, candidate));
        Fixture::check(applied.receipt.water_revision == 5, "water change applied");
        const auto old = owner->read(access, setup.batch);
        Fixture::check(!old && old.error().reason == Sites::Error::stale, "lost notification still reads stale");
        const auto current = Fixture::take(owner->capture(access));
        const auto fresh = Fixture::take(owner->read(access, Fixture::reassess(current.water)));
        print(fresh);
        display_advice(fresh);
        Fixture::check(presentation.get({1}, {1, 1}).value == 2 && presentation.get({0}, {1, 1}).value == 100,
            "real Engine map changes advice without moving the town");
        Fixture::check(Fixture::take(owner->preserved(access)) == setup.preserved, "current town edits debt and knowledge preserved");
        Fixture::check(Fixture::take(owner->adopt(access, candidate)).existing, "lost reply lookup without reapplication");
        std::cout << "historical-water4=Stale; town-cell6-occupied=true; excavation-action61/debt5/knowledge8 preserved; retry=ExistingApplied\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "sites consumer failure: " << failure.what() << '\n';
        return 1;
    }
}
