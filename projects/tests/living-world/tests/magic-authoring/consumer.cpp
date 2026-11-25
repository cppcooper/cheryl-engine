#include <consumer/fixture.h>
#include <magic/authoring/authoring.h>
#include <recovery/paired.h>

#include <iostream>
#include <string>

int main() {
    namespace LivingWorld = CE::LivingWorld;
    try {
        const auto catalog = LivingWorld::fixture_catalog();
        const auto original = LivingWorld::fixture_plan(catalog);
        std::string five(original->source());
        const auto offset = five.find("12 hp");
        if (offset == std::string::npos)
            return 1;
        five.replace(offset, 5, "5 hp");
        for (const bool edited : {false, true}) {
            LivingWorld::Host host(catalog, original);
            const auto access = host.grant(host.entity(1));
            LivingWorld::Scene scene(host, access, host.entity(2));
            LivingWorld::MagicAuthoring authoring(host, access);
            const auto source = edited ? std::string_view(five) : original->source();
            const auto draft = authoring.edit(source);
            if (!draft || !(*draft)->accepted)
                return 2;
            const auto plan = host.inspect(access, *(*draft)->accepted);
            if (!plan || (*plan)->meaning().effects[0].quantity != (edited ? 5u : 12u))
                return 3;
            const auto quote = authoring.quote(host.entity(2));
            if (!quote || quote->body().quote.price != 3)
                return 4;
            const auto action = host.action(access, *quote);
            if (!action || !host.queue(access, *action, host.fence()))
                return 5;
            const auto result = host.settle(access, *action);
            if (!result || result->state != LivingWorld::Disposition::Applied ||
                result->gain != (edited ? 5u : 8u) || host.actor(access, host.entity(1))->reserve != 7)
                return 6;
            const auto invalid = authoring.edit("cercle de feu");
            if (!invalid || (*invalid)->accepted || authoring.action(host.entity(4)))
                return 7;
            const auto explicit_prior = host.action(access, host.entity(4), *(*draft)->accepted);
            if (!explicit_prior || !host.queue(access, *explicit_prior, host.fence()) ||
                host.settle(access, *explicit_prior)->gain != (edited ? 5u : 8u))
                return 8;
            auto cut = LivingWorld::Recovery::capture(host, scene);
            if (!cut)
                return 9;
            auto recovered = LivingWorld::Recovery::restore(*cut, host, access);
            if (!recovered)
                return 10;
            auto& restored = recovered->host();
            const auto player = restored.grant(restored.entity(1));
            if (restored.lookup(player, *action)->gain != result->gain ||
                restored.queue(player, *explicit_prior, host.fence())->gain != (edited ? 5u : 8u) ||
                restored.actor(player, restored.entity(1))->reserve != 4 || restored.deliver().adopted != 2)
                return 11;
            std::cout << "quantity=" << (edited ? 5 : 12) << " gain=" << result->gain
                      << " price=3 retained_after_recovery=true\n";
        }
        return 0;
    } catch (...) {
        return 12;
    }
}
