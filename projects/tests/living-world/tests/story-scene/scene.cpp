#include <consumer/fixture.h>
#include <story/scene/scene.h>

#include <gtest/gtest.h>

#include <stdexcept>

namespace {
    namespace LivingWorld = CE::LivingWorld;
}

TEST(living_scene, presentation_failure_keeps_same_attempt) {
    const auto catalog = LivingWorld::fixture_catalog();
    LivingWorld::Host host(catalog, LivingWorld::fixture_plan(catalog));
    const auto access = host.grant(host.entity(1));
    LivingWorld::Scene scene(host, access, host.entity(2));
    const auto offer = scene.display().offer.value();
    ASSERT_EQ(scene.request(offer)->state, LivingWorld::Disposition::Queued);
    EXPECT_EQ(scene.state(), LivingWorld::SceneState::Waiting);
    EXPECT_EQ(scene.request(offer)->state, LivingWorld::Disposition::Queued);
    ASSERT_EQ(scene.process()->state, LivingWorld::Disposition::Applied);
    const auto failed = scene.refresh([](std::string_view) { throw std::runtime_error("display failed"); });
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), LivingWorld::Error::Preparation);
    EXPECT_EQ(scene.state(), LivingWorld::SceneState::Waiting);
    EXPECT_EQ(scene.request(offer)->state, LivingWorld::Disposition::Applied);
    EXPECT_EQ(scene.process()->state, LivingWorld::Disposition::Applied);
    ASSERT_TRUE(scene.refresh());
    EXPECT_EQ(scene.state(), LivingWorld::SceneState::Helped);
    EXPECT_EQ(host.actor(access, host.entity(1))->reserve, 7);
    EXPECT_EQ(host.actor(access, host.entity(2))->health, 20);
    EXPECT_EQ(host.deliver().adopted, 1);
}

TEST(living_scene, forged_foreign_and_stale_offers) {
    const auto catalog = LivingWorld::fixture_catalog();
    const auto plan = LivingWorld::fixture_plan(catalog);
    LivingWorld::Host host(catalog, plan);
    const auto access = host.grant(host.entity(1));
    LivingWorld::Scene scene(host, access, host.entity(2));
    LivingWorld::Scene other(host, access, host.entity(2));
    EXPECT_EQ(scene.request({}).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(scene.request(other.display().offer.value()).error(), LivingWorld::Error::Unauthorized);
    const auto stale = scene.display().offer.value();
    ASSERT_TRUE(host.set_price(4));
    EXPECT_EQ(scene.request(stale).error(), LivingWorld::Error::Stale);
    EXPECT_EQ(scene.state(), LivingWorld::SceneState::Offered);
    EXPECT_EQ(host.actor(access, host.entity(1))->reserve, 10);
}

TEST(living_scene, capacity_before_navigation) {
    const auto catalog = LivingWorld::fixture_catalog();
    LivingWorld::Host host(catalog, LivingWorld::fixture_plan(catalog));
    const auto access = host.grant(host.entity(1));
    auto wren = host.actor(access, host.entity(3)).value();
    wren.observes = false;
    ASSERT_TRUE(host.edit_actor(wren));
    for (std::size_t index = 0; index < 16; ++index) {
        LivingWorld::Scene scene(host, access, host.entity(2));
        ASSERT_TRUE(scene.request(scene.display().offer.value()));
        ASSERT_TRUE(scene.process());
    }
    LivingWorld::Scene scene(host, access, host.entity(2));
    const auto before = scene.display();
    const auto health = host.actor(access, host.entity(2))->health;
    const auto reserve = host.actor(access, host.entity(1))->reserve;
    const auto failed = scene.request(before.offer.value());
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), LivingWorld::Error::Capacity);
    EXPECT_EQ(scene.state(), LivingWorld::SceneState::Offered);
    EXPECT_EQ(scene.display().text, before.text);
    EXPECT_EQ(host.actor(access, host.entity(2))->health, health);
    EXPECT_EQ(host.actor(access, host.entity(1))->reserve, reserve);
}

TEST(living_scene, required_work_capacity_before_navigation) {
    const auto catalog = LivingWorld::fixture_catalog();
    LivingWorld::Host host(catalog, LivingWorld::fixture_plan(catalog));
    const auto access = host.grant(host.entity(1));
    for (std::uint64_t id = 1; id <= 4; ++id) {
        auto witness = host.actor(access, host.entity(id)).value();
        witness.observes = true;
        ASSERT_TRUE(host.edit_actor(witness));
    }
    for (std::size_t index = 0; index < 4; ++index) {
        auto caster = host.actor(access, host.entity(1)).value();
        auto marin = host.actor(access, host.entity(2)).value();
        caster.reserve = 10;
        marin.health = 12;
        ASSERT_TRUE(host.edit_actor(caster));
        ASSERT_TRUE(host.edit_actor(marin));
        LivingWorld::Scene scene(host, access, marin.ref);
        ASSERT_TRUE(scene.request(scene.display().offer.value()));
        ASSERT_EQ(scene.process()->state, LivingWorld::Disposition::Applied);
    }
    LivingWorld::Scene blocked(host, access, host.entity(2));
    EXPECT_EQ(blocked.request(blocked.display().offer.value()).error(), LivingWorld::Error::Capacity);
    EXPECT_EQ(blocked.state(), LivingWorld::SceneState::Offered);
    EXPECT_EQ(host.read(host.grant(host.entity(3)))->size, 0);
    EXPECT_EQ(host.deliver().adopted, 16);
    EXPECT_EQ(host.read(host.grant(host.entity(3)))->size, 4);
    EXPECT_EQ(host.deliver().adopted, 0);
}
