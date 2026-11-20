#include "fixtures.h"

#include <cheryl/maps/tilemap.h>

#include <iostream>

#if defined(CE_ENGINE_STATIC) || defined(CE_BUILDING_ENGINE)
#error Habitat consumer requires the normal shared Engine import contract.
#endif

int main() {
    namespace Habitat = CE::LivingWorld::Habitat;
    namespace Fixture = Habitat::Fixture;
    try {
        const auto literal = Fixture::literal();
        auto literal_advice = Fixture::advisor(literal);
        const auto middle = Fixture::take(literal_advice->prepare(Fixture::policy(), {2, 0, 1, 2}));
        Fixture::check(middle.places().size() == 1 && middle.places()[0].terrain.index == 7, "wet middle not lost to dry shortlist");
        const auto setup = Fixture::generated();
        auto advice = Fixture::advisor(setup);
        const auto plan = Fixture::take(advice->prepare(Fixture::policy(), {3, 0, 0, 1}));
        static_cast<void>(Fixture::take(advice->read(plan)));
        Fixture::check(plan.places().size() == 1 && plan.selection().unknown_excluded() == 5, "explicit generated evidence coverage");
        std::cout << "eligibility-before-spacing=true; generated-Elevation=true; owned-replay=true; "
                  << "coverage=incomplete; unknown-interior=" << plan.selection().unknown_excluded()
                  << "; requested=" << plan.selection().requested() << "; achieved=" << plan.selection().achieved()
                  << "; greedy-shortfall=" << plan.selection().shortfall()
                  << "; selector-work=" << plan.selection().work() << "; selector-bytes=" << plan.selection().storage_bytes()
                  << "; total-charge-work=" << plan.accounting().work << "; total-data-bytes=" << plan.accounting().storage_bytes
                  << "; resident-slots=" << plan.resident_slots() << "; activity-slots=" << plan.activity_slots()
                  << "; cap=16; not-maximum-packing; not-physical-capacity; no-people-created\n";
        CE::Tilemaps::Map presentation({5, 4}, 2);
        presentation.set({0}, {1, 1}, {100});
        for (const auto& place : plan.places()) {
            presentation.set({1}, {static_cast<std::int64_t>(place.terrain.x), static_cast<std::int64_t>(place.terrain.y)}, {1});
        }
        Fixture::check(presentation.get({1}, {2, 1}).value == 1, "one shared Engine presents real proposal");
        const auto candidate = Fixture::water_change(setup);
        static_cast<void>(Fixture::take(setup.owner->adopt(setup.owner->access(), candidate)));
        const auto stale = advice->read(plan);
        Fixture::check(!stale && stale.error().reason == Habitat::Error::stale, "water adoption invalidates without notification");
        for (const auto& place : plan.places()) {
            presentation.set({1}, {static_cast<std::int64_t>(place.terrain.x), static_cast<std::int64_t>(place.terrain.y)}, {0});
        }
        Fixture::check(presentation.get({1}, {2, 1}).value == 0, "stale proposal is no longer presented as current advice");
        Fixture::check(Fixture::take(setup.owner->preserved(setup.owner->access())) == setup.preserved,
            "existing town edit debt knowledge preserved");
        Fixture::check(presentation.get({0}, {1, 1}).value == 100, "town marker never relocated");
        std::cout << "water-change-read=Stale; stale-overlay=cleared; preserved-world=true; literal-middle=7; graphical/population acceptance remains later\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "habitat consumer failure: " << failure.what() << '\n';
        return 1;
    }
}
