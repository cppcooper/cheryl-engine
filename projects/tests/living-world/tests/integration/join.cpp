#include <consumer/fixture.h>
#include <story/scene/scene.h>

#include <gtest/gtest.h>

#ifdef CE_BUILDING_ENGINE
#error This joined consumer imports the real Engine.
#endif

namespace {
    namespace LivingWorld = CE::LivingWorld;
}

TEST(living_join, dialogue_contact_observation_continuation) {
    const auto catalog = LivingWorld::fixture_catalog();
    const auto plan = LivingWorld::fixture_plan(catalog);
    for (bool willing : {false, true}) {
        LivingWorld::Host host(catalog, plan);
        const auto access = host.grant(host.entity(1));
        auto marin = host.actor(access, host.entity(2)).value();
        marin.willing = willing;
        ASSERT_TRUE(host.edit_actor(marin));
        LivingWorld::Scene scene(host, access, marin.ref);
        ASSERT_TRUE(scene.request(scene.display().offer.value()));
        const auto result = scene.process();
        ASSERT_TRUE(result);
        EXPECT_EQ(result->state, willing ? LivingWorld::Disposition::Applied : LivingWorld::Disposition::Refused);
        EXPECT_EQ(result->reason, willing ? LivingWorld::Reason::None : LivingWorld::Reason::Contact);
        EXPECT_EQ(host.actor(access, marin.ref)->health, willing ? 20 : 12);
        EXPECT_EQ(host.actor(access, host.entity(1))->reserve, willing ? 7 : 10);
        EXPECT_EQ(host.deliver().adopted, willing ? 1 : 0);
        EXPECT_EQ(host.read(host.grant(host.entity(3)))->size, willing ? 1 : 0);
        ASSERT_TRUE(scene.refresh());
        EXPECT_EQ(scene.state(), willing ? LivingWorld::SceneState::Helped : LivingWorld::SceneState::Refused);
    }
}

TEST(living_join, hidden_neighbor_changes_do_not_change_display) {
    const auto catalog = LivingWorld::fixture_catalog();
    const auto plan = LivingWorld::fixture_plan(catalog);
    LivingWorld::Host first(catalog, plan), second(catalog, plan);
    const auto access_first = first.grant(first.entity(1));
    const auto access_second = second.grant(second.entity(1));
    auto private_actor = second.actor(access_second, second.entity(4)).value();
    private_actor.reserve = 999;
    private_actor.can_cast = false;
    ASSERT_TRUE(second.edit_actor(private_actor));
    LivingWorld::Scene scene_first(first, access_first, first.entity(2));
    LivingWorld::Scene scene_second(second, access_second, second.entity(2));
    const auto display_first = scene_first.display();
    const auto display_second = scene_second.display();
    EXPECT_EQ(display_first.state, display_second.state);
    EXPECT_EQ(display_first.text, display_second.text);
    EXPECT_EQ(display_first.choice, display_second.choice);
    EXPECT_EQ(first.read(first.grant(first.entity(3)))->size, second.read(second.grant(second.entity(3)))->size);
}
