#include <contract/fixture.h>
#include <cstdlib>
#include <new>
#include <optional>

namespace {
    std::optional<std::size_t> remaining;
    bool reject{};
}
void* operator new(std::size_t size) {
    if (reject || (remaining && *remaining == 0)) {
        throw std::bad_alloc();
    }
    if (remaining) {
        --*remaining;
    }
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    using namespace StoryFixture;
    Inputs inputs;
    bool reached_success = false;
    for (std::size_t failure = 0; failure < 64; ++failure) {
        remaining = failure;
        auto result = Story::Family::create(inputs.seed());
        remaining.reset();
        if (result) {
            reached_success = true;
            break;
        }
        if (result.error() != Story::Error::Capacity || inputs.objects[0].holder != sera) {
            return 1;
        }
    }
    if (!reached_success) {
        return 2;
    }
    auto owner = family();
    reject = true;
    const auto trace = baseline(*owner);
    reject = false;
    if (!trace.transferred || trace.transferred != trace.duplicate || !trace.told ||
        trace.episodes != 5 || trace.acquisitions != 9) {
        return 3;
    }
    auto growing = family();
    reject = true;
    const auto life = dynamic_life(*growing);
    const auto repeated = growing->apply(growing->life(), life_request(3, 11, Story::Death{life.introduced.person, 1}));
    reject = false;
    if (!life.complete || repeated != life.died || life.newborn_accounts != 0 ||
        growing->object(box)->holder != life.born.person) {
        return 4;
    }
    auto loss = family();
    reject = true;
    const auto pending = StoryFixture::promise(*loss);
    const auto death = loss->apply(loss->life(), life_request(1, 3, Story::Death{sera, 1}));
    reject = false;
    if (!death || loss->commitment(pending)->state != Story::CommitmentState::Failed ||
        loss->commitment(pending)->settlement != death.episode || loss->object(box)->holder != sera) {
        return 5;
    }
    return 0;
}
