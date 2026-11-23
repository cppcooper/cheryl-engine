#include <consumer/fixture.h>
#include <story/scene/scene.h>
#include <recovery/paired.h>

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

    void recover(const ::Magic::Catalog& catalog, const ::Magic::PlanHandle& plan) {
        namespace LivingWorld = CE::LivingWorld;
        LivingWorld::Host original(catalog, plan);
        const auto player = original.grant(original.entity(1));
        LivingWorld::Scene scene(original, player, original.entity(2));
        const auto old_offer = scene.display().offer.value();
        scene.request(old_offer).value();
        const auto applied = scene.process().value();
        auto cut = LivingWorld::Recovery::capture(original, scene).value();
        auto pair = LivingWorld::Recovery::restore(cut, original, player).value();
        auto& host = pair.host();
        auto& restored = pair.scene();
        const auto access = host.grant(host.entity(1));
        const auto stale = restored.request(old_offer);
        if (restored.state() != LivingWorld::SceneState::Waiting ||
            stale || stale.error() != LivingWorld::Error::Unauthorized)
            throw std::runtime_error("Recovery reused live scene authority");
        auto garo = host.actor(access, host.entity(4)).value();
        garo.observes = true;
        host.edit_actor(garo).value();
        restored.refresh().value();
        const auto delivered = host.deliver();
        const auto repeated = restored.process().value();
        const auto retry = host.deliver();
        const auto wren = host.read(host.grant(host.entity(3))).value();
        const auto absent = host.read(host.grant(host.entity(4))).value();
        if (applied.state != LivingWorld::Disposition::Applied || applied != repeated ||
            host.actor(access, host.entity(2))->health != 20 ||
            host.actor(access, host.entity(1))->reserve != 7 || delivered.adopted != 1 ||
            retry.adopted || wren.size != 1 || absent.size != 0)
            throw std::runtime_error("Recovery replayed an effect or invented a witness");
        std::cout << "Recovered without replay: health=20 reserve=7 witness-acquisitions=1\n";
    }
}

int main() {
    try {
        const auto catalog = CE::LivingWorld::fixture_catalog();
        const auto plan = CE::LivingWorld::fixture_plan(catalog);
        run(catalog, plan, true);
        run(catalog, plan, false);
        recover(catalog, plan);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
