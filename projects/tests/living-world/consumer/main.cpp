#include <consumer/fixture.h>
#include <story/scene/scene.h>

#include <iostream>
#include <stdexcept>

namespace {
    void run(const ::Magic::Catalog& catalog, const ::Magic::PlanHandle& plan, bool willing) {
        namespace LivingWorld = CE::LivingWorld;
        LivingWorld::Host host(catalog, plan);
        const auto player = host.grant(host.entity(1));
        auto marin = host.actor(player, host.entity(2)).value();
        marin.willing = willing;
        host.edit_actor(marin).value();
        LivingWorld::Scene scene(host, player, marin.ref);
        auto view = scene.display();
        std::cout << view.text << '\n' << view.choice << '\n';
        scene.request(view.offer.value()).value();
        const auto receipt = scene.process().value();
        static_cast<void>(LivingWorld::Scene::notify({}));
        const auto delivered = host.deliver();
        scene.refresh().value();
        std::cout << scene.display().text << " health=" << host.actor(player, marin.ref)->health
                  << " reserve=" << host.actor(player, host.entity(1))->reserve
                  << " gain=" << receipt.gain << " observations=" << delivered.adopted << '\n';
        const auto expected = willing ? LivingWorld::Disposition::Applied : LivingWorld::Disposition::Refused;
        if (receipt.state != expected)
            throw std::runtime_error("Unexpected joined disposition");
    }
}

int main() {
    try {
        const auto catalog = CE::LivingWorld::fixture_catalog();
        const auto plan = CE::LivingWorld::fixture_plan(catalog);
        run(catalog, plan, true);
        run(catalog, plan, false);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
