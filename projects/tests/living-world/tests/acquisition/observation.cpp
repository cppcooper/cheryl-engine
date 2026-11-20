#include <consumer/fixture.h>
#include <story/scene/scene.h>

#include <gtest/gtest.h>

#include <stdexcept>

namespace {
    namespace LivingWorld = CE::LivingWorld;
}

TEST(living_observation, captured_witness_not_late_neighbor) {
    const auto catalog = LivingWorld::fixture_catalog();
    LivingWorld::Host host(catalog, LivingWorld::fixture_plan(catalog));
    const auto player = host.grant(host.entity(1));
    const auto wren = host.grant(host.entity(3));
    const auto garo = host.grant(host.entity(4));
    const auto action = host.action(player, host.entity(2)).value();
    ASSERT_TRUE(host.queue(player, action, host.fence()));
    ASSERT_EQ(host.settle(player, action)->state, LivingWorld::Disposition::Applied);
    auto late = host.actor(player, host.entity(4)).value();
    late.observes = true;
    ASSERT_TRUE(host.edit_actor(late));
    EXPECT_FALSE(LivingWorld::Scene::notify({}));
    EXPECT_FALSE(LivingWorld::Scene::notify([] { throw std::runtime_error("lost wake"); }));
    EXPECT_EQ(host.deliver().adopted, 1);
    EXPECT_EQ(host.deliver().adopted, 0);
    const auto page = host.read(wren).value();
    ASSERT_EQ(page.size, 1);
    EXPECT_EQ(page.wound_closures[0].id, 1);
    EXPECT_EQ(page.wound_closures[0].subject, host.entity(2));
    EXPECT_EQ(page.wound_closures[0].source, LivingWorld::Source::Eyewitness);
    EXPECT_LE(page.wound_closures[0].observed_at, page.wound_closures[0].acquired_at);
    EXPECT_EQ(host.read(wren, page.next)->size, 0);
    EXPECT_EQ(host.read(garo)->size, 0);
    EXPECT_EQ(host.read(garo, page.next).error(), LivingWorld::Error::Stale);
}

TEST(living_observation, withholding_does_not_drop_required_work) {
    const auto catalog = LivingWorld::fixture_catalog();
    LivingWorld::Host host(catalog, LivingWorld::fixture_plan(catalog));
    const auto player = host.grant(host.entity(1));
    const auto witness = host.grant(host.entity(3));
    const auto action = host.action(player, host.entity(2)).value();
    ASSERT_TRUE(host.queue(player, action, host.fence()));
    ASSERT_TRUE(host.settle(player, action));
    auto wren = host.actor(player, host.entity(3)).value();
    wren.access = false;
    ASSERT_TRUE(host.edit_actor(wren));
    EXPECT_TRUE(host.deliver().withheld);
    EXPECT_EQ(host.read(witness).error(), LivingWorld::Error::Unauthorized);
    wren.access = true;
    ASSERT_TRUE(host.edit_actor(wren));
    EXPECT_EQ(host.deliver().adopted, 1);
    EXPECT_EQ(host.read(witness)->size, 1);
    EXPECT_EQ(host.deliver().adopted, 0);
}
