#include <consumer/fixture.h>
#include <magic/authoring/authoring.h>
#include <tests/host/access.h>

#include <cstdlib>
#include <new>
#include <optional>
#include <string>

namespace {
    std::optional<std::size_t> allocations_until_failure;
    class FailAllocation {
    public:
        explicit FailAllocation(std::size_t after) noexcept { allocations_until_failure = after; }
        ~FailAllocation() { allocations_until_failure.reset(); }
    };
}

void* operator new(std::size_t size) {
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
        const auto plan = LivingWorld::fixture_plan(catalog);
        std::string five(plan->source());
        const auto offset = five.find("12 hp");
        if (offset == std::string::npos)
            return 1;
        five.replace(offset, 5, "5 hp");
        bool reached_success = false;
        for (std::size_t failure = 0; failure < 256; ++failure) {
            LivingWorld::Host host(catalog, plan);
            const auto access = host.grant(host.entity(1));
            LivingWorld::MagicAuthoring authoring(host, access);
            const auto prior = authoring.edit(plan->source());
            if (!prior || !(*prior)->accepted)
                return 2;
            const auto revision = LivingWorld::HostTestAccess::image(host).revision;
            std::expected<LivingWorld::SpellDraftHandle, LivingWorld::Error> result =
                std::unexpected(LivingWorld::Error::Preparation);
            {
                const FailAllocation reject(failure);
                result = authoring.edit(five);
            }
            const auto& image = LivingWorld::HostTestAccess::image(host);
            if (result && (*result)->accepted) {
                if (image.plan_count != 2 || image.revision != revision + 1 ||
                    host.inspect(access, *(*result)->accepted).value()->meaning().effects[0].quantity != 5)
                    return 3;
                reached_success = true;
                break;
            }
            if (image.plan_count != 1 || image.revision != revision || image.record_count ||
                LivingWorld::HostTestAccess::issued(host) || image.actors[0].reserve != 10 ||
                image.actors[1].health != 12 || authoring.action(host.entity(2)) ||
                !host.inspect(access, *(*prior)->accepted))
                return 4;
        }
        if (!reached_success)
            return 5;
        LivingWorld::Host host(catalog, plan);
        const auto access = host.grant(host.entity(1));
        const auto accepted = host.accept(access, five).value();
        {
            const FailAllocation reject(0);
            if (!host.inspect(access, accepted) || !host.quote(access, accepted, host.entity(2)))
                return 6;
            bool failed = false;
            try {
                static_cast<void>(host.action(access, host.entity(2), accepted));
            } catch (const std::bad_alloc&) {
                failed = true;
            }
            if (!failed || LivingWorld::HostTestAccess::issued(host))
                return 7;
        }
        const auto action = host.action(access, host.entity(2), accepted).value();
        {
            const FailAllocation reject(0);
            bool failed = false;
            try {
                static_cast<void>(host.queue(access, action, host.fence()));
            } catch (const std::bad_alloc&) {
                failed = true;
            }
            if (!failed || LivingWorld::HostTestAccess::image(host).record_count ||
                host.actor(access, host.entity(1))->reserve != 10)
                return 8;
        }
        if (!host.queue(access, action, host.fence()))
            return 9;
        {
            const FailAllocation reject(0);
            const auto receipt = host.settle(access, action);
            if (!receipt || receipt->state != LivingWorld::Disposition::Applied || receipt->gain != 5 ||
                host.actor(access, host.entity(1))->reserve != 7 || host.actor(access, host.entity(2))->health != 17 ||
                host.queue(access, action, {99, 99})->gain != 5 || host.delivery(access, action)->outstanding != 1 ||
                host.deliver().adopted != 1 || host.delivery(access, action)->acquired != 1)
                return 10;
        }
        return 0;
    } catch (...) {
        return 11;
    }
}
