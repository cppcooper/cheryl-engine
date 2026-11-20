#include <consumer/fixture.h>
#include <story/scene/scene.h>

#include <cstdlib>
#include <new>

namespace {
    bool reject_allocations = false;
    class RejectAllocations {
    public:
        RejectAllocations() noexcept { reject_allocations = true; }
        ~RejectAllocations() { reject_allocations = false; }
    };
}

void* operator new(std::size_t size) {
    if (reject_allocations)
        throw std::bad_alloc();
    if (void* memory = std::malloc(size ? size : 1))
        return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    namespace LivingWorld = CE::LivingWorld;
    try {
        const auto catalog = LivingWorld::fixture_catalog();
        LivingWorld::Host host(catalog, LivingWorld::fixture_plan(catalog));
        const auto access = host.grant(host.entity(1));
        LivingWorld::Scene scene(host, access, host.entity(2));
        const auto offer = scene.display().offer.value();
        bool reserve_failed = false;
        try {
            const RejectAllocations reject;
            static_cast<void>(scene.request(offer));
        } catch (const std::bad_alloc&) {
            reserve_failed = true;
        }
        if (!reserve_failed || scene.state() != LivingWorld::SceneState::Offered ||
            host.actor(access, host.entity(1))->reserve != 10 || host.actor(access, host.entity(2))->health != 12)
            return 1;
        if (!scene.request(offer))
            return 2;
        {
            const RejectAllocations reject;
            const auto result = scene.process();
            if (!result || result->state != LivingWorld::Disposition::Applied || result->gain != 8)
                return 3;
            const auto failed = scene.refresh();
            if (failed || failed.error() != LivingWorld::Error::Preparation)
                return 4;
            if (scene.state() != LivingWorld::SceneState::Waiting)
                return 5;
            const auto repeated = scene.request(offer);
            if (!repeated || repeated->state != LivingWorld::Disposition::Applied)
                return 6;
            if (host.deliver().adopted != 1)
                return 7;
        }
        if (!scene.refresh() || scene.state() != LivingWorld::SceneState::Helped ||
            host.actor(access, host.entity(1))->reserve != 7 || host.actor(access, host.entity(2))->health != 20)
            return 8;
        return 0;
    } catch (...) {
        return 9;
    }
}
