#include "fixtures.h"

#include <cstdlib>
#include <iostream>
#include <new>

namespace {
    std::size_t fail_at = 0;
    std::size_t allocations = 0;
}
void* operator new(std::size_t bytes) {
    if (fail_at != 0 && ++allocations == fail_at) { throw std::bad_alloc(); }
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
        bool completed = false;
        std::size_t failures = 0;
        for (std::size_t boundary = 1; boundary <= 16 && !completed; ++boundary) {
            bool threw = false;
            allocations = 0;
            fail_at = boundary;
            try {
                const auto candidate = Eligibility::literal(setup.owner, setup.owner->access(), 1,
                    setup.evidence->image(), 2, rules);
                fail_at = 0;
                check(candidate.has_value(), "valid evidence eventually constructs");
                completed = true;
            } catch (const std::bad_alloc&) { threw = true; }
            fail_at = 0;
            if (threw) { ++failures; }
            static_cast<void>(take(advice->read(plan)));
            check(take(setup.owner->preserved(setup.owner->access())) == setup.preserved,
                "each allocation failure preserves world and prior advice");
        }
        check(completed && failures >= 2, "bounded sweep covers partial evidence construction and completion");
        const auto domain = setup.evidence->image().domain();
        const auto cells = setup.evidence->image().cells();
        bool threw = false;
        allocations = 0;
        fail_at = 1;
        try {
            static_cast<void>(ProcGen::EligibilityImage::make(domain, {1, 1, ProcGen::EligibilityCoverage::complete}, cells,
                ProcGen::eligibility_hard_limits));
        } catch (const std::bad_alloc&) { threw = true; }
        fail_at = 0;
        check(threw, "owned cell allocation failure publishes no image");
        return 0;
    } catch (const std::exception& failure) {
        fail_at = 0;
        std::cerr << "habitat allocation failure: " << failure.what() << '\n';
        return 1;
    }
}
