#include <consumer/fixture.h>
#include <host/host.h>
#include <story/scene/scene.h>

#include <gtest/gtest.h>

#include <utility>

namespace CE::LivingWorld {
    struct HostTestAccess {
        static auto reserve(Host& host, const Host::Access& access, const Action& action) {
            return host.reserve(access, action, host.fence());
        }
        static const Detail::Image& image(const Host& host) noexcept { return *host.image_; }
    };
}

namespace {
    namespace LivingWorld = CE::LivingWorld;
    struct HostFixture : testing::Test {
        ::Magic::Catalog catalog = LivingWorld::fixture_catalog();
        ::Magic::PlanHandle plan = LivingWorld::fixture_plan(catalog);
        LivingWorld::Host host{catalog, plan};
        LivingWorld::Host::Access player = host.grant(host.entity(1));
    };
}

TEST_F(HostFixture, clamp_and_pay) {
    const auto action = host.action(player, host.entity(2)).value();
    ASSERT_EQ(host.queue(player, action, host.fence())->state, LivingWorld::Disposition::Queued);
    const auto settled = host.settle(player, action);
    ASSERT_TRUE(settled);
    EXPECT_EQ(settled->state, LivingWorld::Disposition::Applied);
    EXPECT_EQ(settled->gain, 8);
    EXPECT_EQ(host.actor(player, host.entity(2))->health, 20);
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 7);
}

TEST_F(HostFixture, same_plan_other_policy) {
    LivingWorld::Host other(catalog, plan, {2, 1}, 16, 5);
    const auto access = other.grant(other.entity(1));
    const auto action = other.action(access, other.entity(2)).value();
    ASSERT_TRUE(other.queue(access, action, other.fence()));
    const auto settled = other.settle(access, action);
    ASSERT_TRUE(settled);
    EXPECT_EQ(settled->state, LivingWorld::Disposition::Applied);
    EXPECT_EQ(settled->gain, 4);
    EXPECT_EQ(other.actor(access, other.entity(2))->health, 16);
    EXPECT_EQ(other.actor(access, other.entity(1))->reserve, 5);
}

TEST_F(HostFixture, self_cast_composes_fields) {
    const auto action = host.action(player, host.entity(1)).value();
    ASSERT_TRUE(host.queue(player, action, host.fence()));
    const auto settled = host.settle(player, action);
    ASSERT_TRUE(settled);
    EXPECT_EQ(settled->state, LivingWorld::Disposition::Applied);
    const auto self = host.actor(player, host.entity(1)).value();
    EXPECT_EQ(self.health, 20);
    EXPECT_EQ(self.reserve, 7);
}

TEST_F(HostFixture, exact_retries_and_conflicts) {
    const auto action = host.action(player, host.entity(2)).value();
    ASSERT_TRUE(host.queue(player, action, host.fence()));
    EXPECT_EQ(host.queue(player, action, {99, 99})->state, LivingWorld::Disposition::Queued);
    auto changed = action;
    ++changed.body.quote.price;
    const auto conflict = host.queue(player, changed, host.fence());
    ASSERT_FALSE(conflict);
    EXPECT_EQ(conflict.error(), LivingWorld::Error::Conflict);
    ASSERT_EQ(host.settle(player, action)->state, LivingWorld::Disposition::Applied);
    ASSERT_TRUE(host.set_price(9));
    EXPECT_EQ(host.queue(player, action, {99, 99})->state, LivingWorld::Disposition::Applied);
    EXPECT_EQ(host.settle(player, action)->gain, 8);
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 7);
}

TEST_F(HostFixture, refused_is_retained) {
    auto caster = host.actor(player, host.entity(1)).value();
    caster.reserve = 2;
    ASSERT_TRUE(host.edit_actor(caster));
    const auto action = host.action(player, host.entity(2)).value();
    ASSERT_TRUE(host.queue(player, action, host.fence()));
    const auto refused = host.settle(player, action);
    ASSERT_TRUE(refused);
    EXPECT_EQ(refused->state, LivingWorld::Disposition::Refused);
    EXPECT_EQ(refused->reason, LivingWorld::Reason::Resources);
    EXPECT_EQ(host.actor(player, host.entity(2))->health, 12);
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 2);
    caster.reserve = 10;
    ASSERT_TRUE(host.edit_actor(caster));
    EXPECT_EQ(host.queue(player, action, host.fence()).value(), *refused);
    EXPECT_EQ(host.deliver().adopted, 0);
}

TEST_F(HostFixture, permission_precedes_key_existence) {
    const auto action = host.action(player, host.entity(2)).value();
    ASSERT_TRUE(host.queue(player, action, host.fence()));
    ASSERT_TRUE(host.settle(player, action));
    const auto garo = host.grant(host.entity(4));
    const auto foreign = host.lookup(garo, action);
    ASSERT_FALSE(foreign);
    EXPECT_EQ(foreign.error(), LivingWorld::Error::Unauthorized);
    auto actor = host.actor(player, host.entity(1)).value();
    actor.access = false;
    ASSERT_TRUE(host.edit_actor(actor));
    EXPECT_EQ(host.lookup(player, action).error(), LivingWorld::Error::Unauthorized);
}

TEST_F(HostFixture, stale_and_malformed_admission) {
    const auto action = host.action(player, host.entity(2)).value();
    auto old_fence = host.fence();
    ++old_fence.epoch;
    EXPECT_EQ(host.queue(player, action, old_fence).error(), LivingWorld::Error::StaleFence);
    auto duplicate = action;
    duplicate.body.reads.stamps[1] = duplicate.body.reads.stamps[0];
    EXPECT_EQ(host.queue(player, duplicate, host.fence()).error(), LivingWorld::Error::InvalidBody);
    ASSERT_TRUE(host.set_price(4));
    EXPECT_EQ(host.queue(player, action, host.fence()).error(), LivingWorld::Error::Stale);
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 10);
}

TEST_F(HostFixture, domain_change_after_queue) {
    const auto action = host.action(player, host.entity(2)).value();
    ASSERT_TRUE(host.queue(player, action, host.fence()));
    auto marin = host.actor(player, host.entity(2)).value();
    marin.willing = false;
    ASSERT_TRUE(host.edit_actor(marin));
    const auto result = host.settle(player, action);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->state, LivingWorld::Disposition::Refused);
    EXPECT_EQ(result->reason, LivingWorld::Reason::Stale);
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 10);
    EXPECT_EQ(host.actor(player, marin.ref)->health, 12);
    EXPECT_EQ(host.deliver().adopted, 0);
}

TEST_F(HostFixture, recipe_binding_is_not_an_id_alias) {
    std::string source(plan->source());
    const auto quantity = source.find("12 hp");
    ASSERT_NE(quantity, std::string::npos);
    source.replace(quantity, 5, "11 hp");
    const auto changed_plan = ::Magic::compile(source, catalog);
    ASSERT_TRUE(changed_plan);
    LivingWorld::Host other(catalog, *changed_plan);
    const auto access = other.grant(other.entity(1));
    const auto original = host.action(player, host.entity(2)).value();
    const auto other_action = other.action(access, other.entity(2)).value();
    ASSERT_EQ(original.key, other_action.key);
    EXPECT_NE(original.body.plan, other_action.body.plan);
    EXPECT_EQ(other.queue(access, original, other.fence()).error(), LivingWorld::Error::Stale);
    EXPECT_EQ(other.actor(access, other.entity(1))->reserve, 10);
}

TEST_F(HostFixture, changed_live_dependencies_refuse_without_effects) {
    for (unsigned dependency = 0; dependency < 4; ++dependency) {
        LivingWorld::Host owner(catalog, plan);
        const auto access = owner.grant(owner.entity(1));
        const auto action = owner.action(access, owner.entity(2)).value();
        ASSERT_TRUE(owner.queue(access, action, owner.fence()));
        if (dependency == 0) {
            ASSERT_TRUE(owner.set_price(4));
        } else {
            auto actor = owner.actor(access, owner.entity(dependency == 1 ? 1 : 2)).value();
            if (dependency == 1)
                actor.reserve = 2;
            if (dependency == 2)
                actor.touching = false;
            if (dependency == 3)
                actor.eligible = false;
            ASSERT_TRUE(owner.edit_actor(actor));
        }
        const auto before_caster = owner.actor(access, owner.entity(1)).value();
        const auto before_marin = owner.actor(access, owner.entity(2)).value();
        const auto result = owner.settle(access, action);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->state, LivingWorld::Disposition::Refused);
        EXPECT_EQ(result->reason, LivingWorld::Reason::Stale);
        EXPECT_EQ(owner.actor(access, owner.entity(1)).value(), before_caster);
        EXPECT_EQ(owner.actor(access, owner.entity(2)).value(), before_marin);
        EXPECT_EQ(owner.deliver().adopted, 0);
    }
}

TEST_F(HostFixture, retained_capacity_no_eviction) {
    auto wren = host.actor(player, host.entity(3)).value();
    wren.observes = false;
    ASSERT_TRUE(host.edit_actor(wren));
    std::optional<LivingWorld::Action> first;
    for (std::size_t index = 0; index < 16; ++index) {
        auto caster = host.actor(player, host.entity(1)).value();
        caster.reserve = 10;
        ASSERT_TRUE(host.edit_actor(caster));
        const auto action = host.action(player, host.entity(2)).value();
        if (!first)
            first = action;
        ASSERT_TRUE(host.queue(player, action, host.fence()));
        ASSERT_EQ(host.settle(player, action)->state, LivingWorld::Disposition::Applied);
    }
    const auto overflow = host.action(player, host.entity(2)).value();
    EXPECT_EQ(host.queue(player, overflow, host.fence()).error(), LivingWorld::Error::Capacity);
    EXPECT_EQ(host.lookup(player, *first)->state, LivingWorld::Disposition::Applied);
}

TEST_F(HostFixture, older_offer_after_refusal) {
    auto marin = host.actor(player, host.entity(2)).value();
    marin.willing = false;
    ASSERT_TRUE(host.edit_actor(marin));
    const auto caster_before = host.actor(player, host.entity(1)).value();
    const auto marin_before = host.actor(player, marin.ref).value();
    LivingWorld::Scene older(host, player, marin.ref), newer(host, player, marin.ref);
    const auto older_display = older.display();
    ASSERT_TRUE(older_display.offer);
    ASSERT_EQ(older_display.choice, "Help Marin");
    const auto newer_offer = newer.display().offer.value();
    ASSERT_EQ(newer.request(newer_offer)->state, LivingWorld::Disposition::Queued);
    EXPECT_EQ(newer.request(newer_offer)->state, LivingWorld::Disposition::Queued);
    const auto busy = older.request(*older_display.offer);
    ASSERT_FALSE(busy);
    EXPECT_EQ(busy.error(), LivingWorld::Error::Busy);
    EXPECT_EQ(older.state(), LivingWorld::SceneState::Offered);
    const LivingWorld::Receipt refusal{LivingWorld::Disposition::Refused, LivingWorld::Reason::Contact, 0};
    ASSERT_EQ(newer.process().value(), refusal);
    EXPECT_EQ(host.actor(player, host.entity(1)).value(), caster_before);
    EXPECT_EQ(host.actor(player, marin.ref).value(), marin_before);
    EXPECT_EQ(older.display().choice, older_display.choice);
    ASSERT_TRUE(older.display().offer);
    ASSERT_EQ(older.request(*older_display.offer)->state, LivingWorld::Disposition::Queued);
    EXPECT_EQ(older.request(*older_display.offer)->state, LivingWorld::Disposition::Queued);
    ASSERT_EQ(older.process().value(), refusal);
    const auto& image = LivingWorld::HostTestAccess::image(host);
    ASSERT_EQ(image.record_count, 2);
    EXPECT_GT(image.records[0].action.key.sequence, image.records[1].action.key.sequence);
    EXPECT_NE(image.records[0].action.key, image.records[1].action.key);
    EXPECT_EQ(image.records[0].action.body, image.records[1].action.body);
    EXPECT_EQ(host.lookup(player, image.records[0].action).value(), refusal);
    EXPECT_EQ(host.lookup(player, image.records[1].action).value(), refusal);
    EXPECT_EQ(image.work_count, 0);
    EXPECT_EQ(host.deliver().adopted, 0);
    EXPECT_EQ(host.read(host.grant(host.entity(3)))->size, 0);
    ASSERT_TRUE(newer.refresh());
    ASSERT_TRUE(older.refresh());
    EXPECT_EQ(newer.state(), LivingWorld::SceneState::Refused);
    EXPECT_EQ(older.state(), LivingWorld::SceneState::Refused);
    EXPECT_EQ(newer.request(newer_offer).value(), refusal);
    EXPECT_EQ(newer.process().value(), refusal);
    EXPECT_EQ(older.request(*older_display.offer).value(), refusal);
    EXPECT_EQ(older.process().value(), refusal);
    EXPECT_EQ(host.actor(player, host.entity(1)).value(), caster_before);
    EXPECT_EQ(host.actor(player, marin.ref).value(), marin_before);
    EXPECT_EQ(LivingWorld::HostTestAccess::image(host).record_count, 2);
    EXPECT_EQ(host.deliver().adopted, 0);
}

TEST_F(HostFixture, cancellation_is_per_attempt) {
    const auto older = host.action(player, host.entity(2)).value();
    const auto newer = host.action(player, host.entity(2)).value();
    const auto caster_before = host.actor(player, host.entity(1)).value();
    const auto marin_before = host.actor(player, host.entity(2)).value();
    {
        auto reservation = LivingWorld::HostTestAccess::reserve(host, player, newer);
        ASSERT_TRUE(reservation);
        auto moved = std::move(*reservation);
        EXPECT_EQ(host.queue(player, older, host.fence()).error(), LivingWorld::Error::Busy);
        EXPECT_EQ(host.edit_actor(caster_before).error(), LivingWorld::Error::Busy);
        EXPECT_EQ(host.deliver().adopted, 0);
        EXPECT_EQ(LivingWorld::HostTestAccess::image(host).record_count, 0);
    }
    EXPECT_EQ(LivingWorld::HostTestAccess::image(host).record_count, 1);
    EXPECT_EQ(host.lookup(player, newer).error(), LivingWorld::Error::InvalidBody);
    EXPECT_EQ(host.queue(player, newer, host.fence()).error(), LivingWorld::Error::InvalidBody);
    auto changed = newer;
    ++changed.body.quote.price;
    EXPECT_EQ(host.queue(player, changed, host.fence()).error(), LivingWorld::Error::Conflict);
    EXPECT_EQ(host.actor(player, host.entity(1)).value(), caster_before);
    EXPECT_EQ(host.actor(player, host.entity(2)).value(), marin_before);
    ASSERT_EQ(host.queue(player, older, host.fence())->state, LivingWorld::Disposition::Queued);
    ASSERT_EQ(host.settle(player, older)->state, LivingWorld::Disposition::Applied);
    EXPECT_EQ(host.queue(player, newer, host.fence()).error(), LivingWorld::Error::InvalidBody);
    EXPECT_EQ(host.queue(player, older, host.fence())->state, LivingWorld::Disposition::Applied);
}

TEST_F(HostFixture, cancellations_exhaust_capacity) {
    LivingWorld::Scene older(host, player, host.entity(2));
    const auto display = older.display();
    ASSERT_TRUE(display.offer);
    std::optional<LivingWorld::Action> first;
    for (std::size_t index = 0; index < 16; ++index) {
        const auto action = host.action(player, host.entity(2)).value();
        if (!first)
            first = action;
        auto reservation = LivingWorld::HostTestAccess::reserve(host, player, action);
        ASSERT_TRUE(reservation);
    }
    const auto& image = LivingWorld::HostTestAccess::image(host);
    EXPECT_EQ(image.record_count, 16);
    EXPECT_EQ(image.work_count, 0);
    EXPECT_EQ(host.queue(player, *first, host.fence()).error(), LivingWorld::Error::InvalidBody);
    auto changed = *first;
    ++changed.body.quote.price;
    EXPECT_EQ(host.lookup(player, changed).error(), LivingWorld::Error::Conflict);
    const auto overflow = older.request(*display.offer);
    ASSERT_FALSE(overflow);
    EXPECT_EQ(overflow.error(), LivingWorld::Error::Capacity);
    EXPECT_EQ(older.state(), LivingWorld::SceneState::Offered);
    EXPECT_EQ(older.display().text, display.text);
    EXPECT_EQ(older.display().choice, display.choice);
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 10);
    EXPECT_EQ(host.actor(player, host.entity(2))->health, 12);
    EXPECT_EQ(host.deliver().adopted, 0);
    EXPECT_EQ(LivingWorld::HostTestAccess::image(host).record_count, 16);
}
