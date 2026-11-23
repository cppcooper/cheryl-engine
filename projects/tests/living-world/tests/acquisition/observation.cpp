#include <consumer/fixture.h>
#include <story/scene/scene.h>
#include <tests/host/access.h>

#include <gtest/gtest.h>

#include <cstring>
#include <stdexcept>
#include <vector>

namespace {
    namespace LivingWorld = CE::LivingWorld;
    auto snapshot(const LivingWorld::Host& host) {
        const auto& image = LivingWorld::HostTestAccess::image(host);
        std::vector<unsigned char> bytes(sizeof(image));
        std::memcpy(bytes.data(), &image, bytes.size());
        return bytes;
    }
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

TEST(living_observation, exact_delivery_authenticates_without_mutation) {
    const auto catalog = LivingWorld::fixture_catalog();
    const auto plan = LivingWorld::fixture_plan(catalog);
    LivingWorld::Host host(catalog, plan), foreign(catalog, plan);
    const auto player = host.grant(host.entity(1));
    const auto witness = host.grant(host.entity(3));
    const auto foreign_player = foreign.grant(foreign.entity(1));
    const auto action = host.action(player, host.entity(2)).value();
    const auto unqueued = snapshot(host);
    EXPECT_EQ(host.delivery(player, action).error(), LivingWorld::Error::Missing);
    EXPECT_EQ(snapshot(host), unqueued);
    ASSERT_TRUE(host.queue(player, action, host.fence()));
    const auto before = snapshot(host);
    auto wrong_body = action;
    ++wrong_body.body.quote.price;
    auto wrong_scope = action;
    ++wrong_scope.key.scope.branch;
    auto wrong_issuer = action;
    wrong_issuer.key.issuer = 3;
    EXPECT_EQ(host.delivery({}, action).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(host.delivery(foreign_player, action).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(host.delivery(witness, action).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(host.delivery(player, wrong_scope).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(host.delivery(player, wrong_issuer).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(host.delivery(player, wrong_body).error(), LivingWorld::Error::Conflict);
    const auto queued = host.delivery(player, action);
    ASSERT_TRUE(queued);
    EXPECT_EQ(queued->receipt.state, LivingWorld::Disposition::Queued);
    EXPECT_EQ(queued->required, 0u); EXPECT_EQ(queued->acquired, 0u); EXPECT_EQ(queued->outstanding, 0u);
    EXPECT_EQ(snapshot(host), before);
    ASSERT_EQ(host.settle(player, action)->state, LivingWorld::Disposition::Applied);
    const auto unknown = host.action(player, host.entity(4)).value();
    const auto settled = snapshot(host);
    EXPECT_EQ(host.delivery(player, unknown).error(), LivingWorld::Error::Missing);
    EXPECT_EQ(snapshot(host), settled);
    {
        auto reservation = LivingWorld::HostTestAccess::reserve(host, player, unknown);
        ASSERT_TRUE(reservation);
    }
    const auto cancelled = snapshot(host);
    EXPECT_EQ(host.delivery(player, unknown).error(), LivingWorld::Error::InvalidBody);
    auto changed_cancelled = unknown;
    ++changed_cancelled.body.quote.price;
    EXPECT_EQ(host.delivery(player, changed_cancelled).error(), LivingWorld::Error::Conflict);
    EXPECT_EQ(snapshot(host), cancelled);
    EXPECT_EQ(LivingWorld::HostTestAccess::image(host).record_count, 2u);
    EXPECT_EQ(LivingWorld::HostTestAccess::image(host).acquired_count, 0u);
    EXPECT_EQ(host.actor(player, host.entity(2))->health, 20u);
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 7u);
}

TEST(living_observation, exact_delivery_keeps_cumulative_captured_witnesses) {
    const auto catalog = LivingWorld::fixture_catalog();
    LivingWorld::Host host(catalog, LivingWorld::fixture_plan(catalog));
    const auto player = host.grant(host.entity(1));
    auto wren = host.actor(player, host.entity(3)).value();
    auto garo = host.actor(player, host.entity(4)).value();
    wren.access = false; garo.observes = true;
    ASSERT_TRUE(host.edit_actor(wren)); ASSERT_TRUE(host.edit_actor(garo));
    const auto action = host.action(player, host.entity(2)).value();
    ASSERT_TRUE(host.queue(player, action, host.fence())); ASSERT_TRUE(host.settle(player, action));
    const auto before = snapshot(host);
    const auto required = host.delivery(player, action);
    ASSERT_TRUE(required);
    EXPECT_EQ(required->required, 2u); EXPECT_EQ(required->acquired, 0u); EXPECT_EQ(required->outstanding, 2u);
    EXPECT_EQ(snapshot(host), before);
    const auto partial = host.deliver();
    EXPECT_EQ(partial.adopted, 1u); EXPECT_TRUE(partial.withheld);
    wren.observes = false; garo.observes = false; garo.access = false;
    ASSERT_TRUE(host.edit_actor(wren)); ASSERT_TRUE(host.edit_actor(garo));
    const auto after_partial = snapshot(host);
    const auto pending = host.delivery(player, action);
    ASSERT_TRUE(pending);
    EXPECT_EQ(pending->required, 2u); EXPECT_EQ(pending->acquired, 1u); EXPECT_EQ(pending->outstanding, 1u);
    EXPECT_EQ(snapshot(host), after_partial);
    wren.access = true;
    ASSERT_TRUE(host.edit_actor(wren));
    EXPECT_EQ(host.deliver().adopted, 1u);
    const auto after = snapshot(host);
    const auto complete = host.delivery(player, action);
    ASSERT_TRUE(complete);
    EXPECT_EQ(complete->receipt.state, LivingWorld::Disposition::Applied);
    EXPECT_EQ(complete->required, 2u); EXPECT_EQ(complete->acquired, 2u); EXPECT_EQ(complete->outstanding, 0u);
    EXPECT_EQ(host.delivery(player, action)->acquired, 2u);
    EXPECT_EQ(snapshot(host), after);
    EXPECT_EQ(host.deliver().adopted, 0u);
    EXPECT_EQ(host.queue(player, action, host.fence())->gain, 8u);
    EXPECT_EQ(host.settle(player, action)->gain, 8u);
    EXPECT_EQ(host.delivery(player, action)->acquired, 2u);
    const auto& image = LivingWorld::HostTestAccess::image(host);
    EXPECT_EQ(image.record_count, 1u); EXPECT_EQ(image.work_count, 2u); EXPECT_EQ(image.acquired_count, 2u);
    EXPECT_EQ(host.actor(player, host.entity(2))->health, 20u);
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 7u);
}

TEST(living_observation, exact_delivery_zero_gain_and_refusal_require_nothing) {
    const auto catalog = LivingWorld::fixture_catalog();
    LivingWorld::Host host(catalog, LivingWorld::fixture_plan(catalog));
    const auto player = host.grant(host.entity(1));
    auto wren = host.actor(player, host.entity(3)).value();
    wren.access = false;
    ASSERT_TRUE(host.edit_actor(wren));
    const auto first = host.action(player, host.entity(2)).value();
    ASSERT_TRUE(host.queue(player, first, host.fence())); ASSERT_TRUE(host.settle(player, first));
    const auto zero = host.action(player, host.entity(2)).value();
    ASSERT_TRUE(host.queue(player, zero, host.fence()));
    ASSERT_EQ(host.settle(player, zero)->state, LivingWorld::Disposition::Applied);
    auto garo = host.actor(player, host.entity(4)).value(); garo.willing = false;
    ASSERT_TRUE(host.edit_actor(garo));
    const auto refused = host.action(player, host.entity(4)).value();
    ASSERT_TRUE(host.queue(player, refused, host.fence()));
    ASSERT_EQ(host.settle(player, refused)->state, LivingWorld::Disposition::Refused);
    const auto before = snapshot(host);
    for (const auto& action : {zero, refused}) {
        const auto status = host.delivery(player, action);
        ASSERT_TRUE(status);
        EXPECT_EQ(status->receipt.gain, 0u);
        EXPECT_EQ(status->required, 0u); EXPECT_EQ(status->acquired, 0u); EXPECT_EQ(status->outstanding, 0u);
    }
    EXPECT_EQ(host.delivery(player, first)->outstanding, 1u);
    EXPECT_EQ(snapshot(host), before);
    EXPECT_TRUE(host.deliver().withheld);
    const auto& image = LivingWorld::HostTestAccess::image(host);
    EXPECT_EQ(image.record_count, 3u); EXPECT_EQ(image.work_count, 1u); EXPECT_EQ(image.acquired_count, 0u);
    EXPECT_FALSE(image.work[0].consumed);
    EXPECT_EQ(host.actor(player, host.entity(2))->health, 20u);
    EXPECT_EQ(host.actor(player, host.entity(4))->health, 12u);
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 4u);
}

TEST(living_observation, exact_delivery_does_not_hide_missing_acquisition) {
    const auto catalog = LivingWorld::fixture_catalog();
    LivingWorld::Host host(catalog, LivingWorld::fixture_plan(catalog));
    const auto player = host.grant(host.entity(1));
    const auto action = host.action(player, host.entity(2)).value();
    ASSERT_TRUE(host.queue(player, action, host.fence())); ASSERT_TRUE(host.settle(player, action));
    auto& image = const_cast<LivingWorld::Detail::Image&>(LivingWorld::HostTestAccess::image(host));
    ASSERT_EQ(image.work_count, 1u);
    image.work[0].consumed = true;
    const auto malformed = snapshot(host);
    EXPECT_EQ(host.delivery(player, action).error(), LivingWorld::Error::Preparation);
    EXPECT_EQ(snapshot(host), malformed);
    image.work[0].consumed = false;
    EXPECT_EQ(host.delivery(player, action)->outstanding, 1u);
    EXPECT_EQ(host.deliver().adopted, 1u);
    EXPECT_EQ(host.delivery(player, action)->acquired, 1u);
}
