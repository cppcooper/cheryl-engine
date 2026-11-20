#include <consumer/fixture.h>
#include <story/scene/scene.h>
#include <tests/host/access.h>
#include <recovery/paired.h>

#include <cstdlib>
#include <new>
#include <optional>
#include <utility>

namespace {
    bool reject_allocations = false;
    std::optional<std::size_t> allocations_until_failure;
    class FailAllocation {
    public:
        explicit FailAllocation(std::size_t after) noexcept { allocations_until_failure = after; }
        ~FailAllocation() { allocations_until_failure.reset(); }
    };
    class RejectAllocations {
    public:
        RejectAllocations() noexcept { reject_allocations = true; }
        ~RejectAllocations() { reject_allocations = false; }
    };
}

void* operator new(std::size_t size) {
    if (reject_allocations)
        throw std::bad_alloc();
    if (allocations_until_failure) {
        if (*allocations_until_failure == 0)
            throw std::bad_alloc();
        --*allocations_until_failure;
    }
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
            host.actor(access, host.entity(1))->reserve != 10 || host.actor(access, host.entity(2))->health != 12 ||
            LivingWorld::HostTestAccess::image(host).record_count != 0)
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
        LivingWorld::Host cancellation_host(catalog, LivingWorld::fixture_plan(catalog));
        const auto cancellation_access = cancellation_host.grant(cancellation_host.entity(1));
        const auto older = cancellation_host.action(cancellation_access, cancellation_host.entity(2)).value();
        const auto newer = cancellation_host.action(cancellation_access, cancellation_host.entity(2)).value();
        const auto cancellation_revision = LivingWorld::HostTestAccess::image(cancellation_host).revision;
        {
            std::optional<RejectAllocations> reject;
            auto reservation = LivingWorld::HostTestAccess::reserve(cancellation_host, cancellation_access, newer);
            if (!reservation)
                return 10;
            reject.emplace();
            auto moved = std::move(*reservation);
            static_cast<void>(moved);
        }
        {
            const RejectAllocations reject;
            const auto cancelled = cancellation_host.queue(cancellation_access, newer, cancellation_host.fence());
            if (cancelled || cancelled.error() != LivingWorld::Error::InvalidBody ||
                LivingWorld::HostTestAccess::image(cancellation_host).record_count != 1 ||
                LivingWorld::HostTestAccess::image(cancellation_host).revision != cancellation_revision + 1 ||
                !LivingWorld::HostTestAccess::image(cancellation_host).records[0].cancelled ||
                cancellation_host.actor(cancellation_access, cancellation_host.entity(1))->reserve != 10 ||
                cancellation_host.actor(cancellation_access, cancellation_host.entity(2))->health != 12)
                return 11;
        }
        auto prepared = LivingWorld::HostTestAccess::reserve(cancellation_host, cancellation_access, older);
        if (!prepared)
            return 12;
        {
            const RejectAllocations reject;
            prepared->finalize();
            const auto result = cancellation_host.settle(cancellation_access, older);
            if (!result || result->state != LivingWorld::Disposition::Applied || result->gain != 8)
                return 13;
        }
        {
            const RejectAllocations reject;
            const auto& image = LivingWorld::HostTestAccess::image(cancellation_host);
            const auto retained = cancellation_host.lookup(cancellation_access, older);
            const auto cancelled = cancellation_host.queue(cancellation_access, newer, cancellation_host.fence());
            if (image.record_count != 2 || !image.records[0].cancelled || image.records[1].cancelled ||
                image.revision != cancellation_revision + 3 || !retained ||
                retained->state != LivingWorld::Disposition::Applied || cancelled ||
                cancelled.error() != LivingWorld::Error::InvalidBody)
                return 14;
        }
        LivingWorld::Host recovery_host(catalog, LivingWorld::fixture_plan(catalog));
        const auto recovery_access = recovery_host.grant(recovery_host.entity(1));
        LivingWorld::Scene recovery_scene(recovery_host, recovery_access, recovery_host.entity(2));
        const auto recovery_offer = recovery_scene.display().offer.value();
        if (!recovery_scene.request(recovery_offer) || !recovery_scene.process())
            return 15;
        const auto baseline = LivingWorld::HostTestAccess::image(recovery_host).revision;
        bool captured = false;
        for (std::size_t allocation = 0; allocation < 128; ++allocation) {
            const FailAllocation fail(allocation);
            const auto result = LivingWorld::Recovery::capture(recovery_host, recovery_scene);
            if (result) {
                captured = true;
                break;
            }
            if (result.error() != LivingWorld::Error::Preparation ||
                recovery_scene.state() != LivingWorld::SceneState::Waiting ||
                LivingWorld::HostTestAccess::image(recovery_host).revision != baseline)
                return 16;
        }
        if (!captured)
            return 17;
        const auto cut = LivingWorld::Recovery::capture(recovery_host, recovery_scene);
        if (!cut)
            return 18;
        bool restored = false;
        std::size_t failures = 0;
        for (std::size_t allocation = 0; allocation < 128; ++allocation) {
            const FailAllocation fail(allocation);
            const auto result = LivingWorld::Recovery::restore(*cut, recovery_host, recovery_access);
            if (result) {
                restored = true;
                break;
            }
            ++failures;
            if (result.error() != LivingWorld::Error::Preparation ||
                recovery_scene.state() != LivingWorld::SceneState::Waiting ||
                LivingWorld::HostTestAccess::image(recovery_host).revision != baseline ||
                recovery_host.actor(recovery_access, recovery_host.entity(1))->reserve != 7 ||
                recovery_host.actor(recovery_access, recovery_host.entity(2))->health != 20)
                return 19;
        }
        if (!restored || failures < 2)
            return 20;
        auto recovered = LivingWorld::Recovery::restore(*cut, recovery_host, recovery_access);
        if (!recovered || !recovered->scene().refresh() || recovered->host().deliver().adopted != 1 ||
            recovered->host().deliver().adopted != 0 || !recovery_scene.refresh() ||
            recovery_host.deliver().adopted != 1)
            return 21;
        return 0;
    } catch (...) {
        return 9;
    }
}
