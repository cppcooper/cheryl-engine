#include "fixture.h"
#include <iostream>

int main() {
    auto world = ActorFixture::response_world(true);
    ActorFixture::response_offer(world);
    const auto trace = ActorConsumer::choose_response(world.endpoint, {1});
    if (!trace || trace->pending.outcome != CE::Story::ActorOutcome::Pending) { return 1; }
    const auto before = world.host->owner().episode_count();
    if (!world.host->progress(world.endpoint)) { return 2; }
    const auto outcome = ActorConsumer::observe(world.endpoint, {1});
    if (!outcome || !outcome->complete || outcome->disposition.outcome != CE::Story::ActorOutcome::Completed ||
        world.host->owner().episode_count() != before + 1) { return 3; }
    std::cout << "Property: actor-only caller reaches the real owner through retained delivery.\n"
        << "Result: one private response completes once at the host public boundary.\n"
        << "Limits: literal regression, not generation or runtime acceptance of F0.\n";
    return 0;
}
