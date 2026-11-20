#include "fixtures.h"

#include <cstdlib>
#include <iostream>
#include <new>

namespace {
    bool fail_next = false;
}
void* operator new(std::size_t bytes) {
    if (fail_next) { fail_next = false; throw std::bad_alloc(); }
    if (void* memory = std::malloc(bytes == 0 ? 1 : bytes)) { return memory; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    using namespace CE::LivingWorld::Habitat;
    using namespace Fixture;
    try {
        const auto setup = literal();
        auto advice = advisor(setup);
        const auto plan = take(advice->prepare(policy(), {2, 0, 1, 2}));
        const auto rules = seed();
        bool threw = false;
        fail_next = true;
        try {
            static_cast<void>(Eligibility::literal(setup.owner, setup.owner->access(), 1, setup.evidence->image(), 2, rules));
        } catch (const std::bad_alloc&) { threw = true; }
        fail_next = false;
        check(threw, "evidence allocation failure propagates without publication");
        static_cast<void>(take(advice->read(plan)));
        check(take(setup.owner->preserved(setup.owner->access())) == setup.preserved, "allocation failure preserves world and prior advice");
        const auto domain = setup.evidence->image().domain();
        const auto cells = setup.evidence->image().cells();
        threw = false;
        fail_next = true;
        try {
            static_cast<void>(ProcGen::EligibilityImage::make(domain, {1, 1, ProcGen::EligibilityCoverage::complete}, cells,
                ProcGen::eligibility_hard_limits));
        } catch (const std::bad_alloc&) { threw = true; }
        fail_next = false;
        check(threw, "owned cell allocation failure publishes no image");
        return 0;
    } catch (const std::exception& failure) {
        fail_next = false;
        std::cerr << "habitat allocation failure: " << failure.what() << '\n';
        return 1;
    }
}
