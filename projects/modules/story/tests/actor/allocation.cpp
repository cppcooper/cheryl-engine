#include "fixture.h"
#include <cstdlib>
#include <new>

namespace { bool reject{}; }
void* operator new(std::size_t size) {
    if (reject) { throw std::bad_alloc(); }
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) { return memory; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    using namespace ActorFixture;
    auto family = StoryFixture::family();
    reject = true;
    auto failed_create = Story::ActorCoordinator::create(std::move(family), {0});
    reject = false;
    if (failed_create || failed_create.error().error != Story::Error::Capacity) { return 1; }
    auto world = response_world();
    const auto offer = response_offer(world);
    reject = true;
    const auto failed_open = world.host->open(StoryFixture::oren);
    reject = false;
    if (failed_open || failed_open.error().error != Story::Error::Capacity) { return 2; }
    if (!world.host->open(StoryFixture::oren)) { return 3; }
    reject = true;
    const auto pending = world.endpoint.submit({1}, choice(offer));
    const auto gate = world.host->delivery(world.endpoint, false);
    const auto held = world.host->progress(world.endpoint);
    const auto enabled = world.host->delivery(world.endpoint, true);
    const auto delivered = world.host->progress(world.endpoint);
    const auto view = world.endpoint.view();
    const auto receipt = world.endpoint.lookup({1});
    const auto retry = world.endpoint.submit({1}, choice(offer));
    reject = false;
    if (!pending || !gate || held || !held.error().after_adoption || !enabled || !delivered ||
        !view || !receipt || !retry || *receipt != *retry || receipt->outcome != Story::ActorOutcome::Completed ||
        world.host->owner().episode_count() != 3) { return 4; }
    return 0;
}
