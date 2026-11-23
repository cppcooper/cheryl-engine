#include <consumer/fixture.h>
#include <host/host.h>
#include <magic/contact/view.h>

#include <gtest/gtest.h>

namespace {
    namespace LivingWorld = CE::LivingWorld;
    using ProducerError = ::Magic::Contact::Error;
}

TEST(living_contact, real_plan_and_refusing_candidate) {
    const auto catalog = LivingWorld::fixture_catalog();
    const auto plan = LivingWorld::fixture_plan(catalog);
    ASSERT_TRUE(::Magic::Contact::supports(plan));
    ASSERT_EQ(plan->meaning().effects.size(), 1);
    EXPECT_EQ(plan->meaning().effects[0].quantity, 12);
    LivingWorld::Detail::Image image;
    image.actor_count = 2;
    image.actors[0].ref = {{}, 1, 1};
    image.actors[1].ref = {{}, 2, 1};
    LivingWorld::Detail::ContactView view(image, catalog);
    const ::Magic::Contact::Request request{
        LivingWorld::Detail::ContactView::entity(image.actors[0].ref),
        LivingWorld::Detail::ContactView::entity(image.actors[1].ref)
    };
    const auto evaluated = ::Magic::Contact::evaluate(plan, request, view);
    ASSERT_TRUE(evaluated);
    EXPECT_EQ(evaluated->change.health, 20);
    EXPECT_EQ(evaluated->change.resource, 7);
    EXPECT_EQ(view.candidate(evaluated->change).error(), ProducerError::Preparation);
    EXPECT_EQ(image.actors[1].health, 12);
}

TEST(living_contact, each_refusal_reaches_its_guard) {
    const auto catalog = LivingWorld::fixture_catalog();
    const auto plan = LivingWorld::fixture_plan(catalog);
    for (unsigned guard = 0; guard < 5; ++guard) {
        LivingWorld::Host host(catalog, plan);
        const auto access = host.grant(host.entity(1));
        auto caster = host.actor(access, host.entity(1)).value();
        auto marin = host.actor(access, host.entity(2)).value();
        LivingWorld::Reason expected = LivingWorld::Reason::Contact;
        if (guard == 0)
            marin.willing = false;
        if (guard == 1)
            marin.touching = false;
        if (guard == 2) {
            caster.reserve = 2;
            expected = LivingWorld::Reason::Resources;
        }
        if (guard == 3) {
            caster.can_cast = false;
            expected = LivingWorld::Reason::Permission;
        }
        if (guard == 4) {
            marin.cap = 0;
            expected = LivingWorld::Reason::Schema;
        }
        ASSERT_TRUE(host.edit_actor(caster));
        ASSERT_TRUE(host.edit_actor(marin));
        const auto action = host.action(access, marin.ref).value();
        ASSERT_TRUE(host.queue(access, action, host.fence()));
        const auto receipt = host.settle(access, action);
        ASSERT_TRUE(receipt);
        EXPECT_EQ(receipt->state, LivingWorld::Disposition::Refused);
        EXPECT_EQ(receipt->reason, expected);
        EXPECT_EQ(host.actor(access, marin.ref)->health, 12);
        EXPECT_EQ(host.actor(access, caster.ref)->reserve, caster.reserve);
        EXPECT_EQ(host.deliver().adopted, 0);
    }
}

TEST(living_contact, schema_and_generation_invalidate_queued_action) {
    const auto catalog = LivingWorld::fixture_catalog();
    const auto plan = LivingWorld::fixture_plan(catalog);
    for (bool schema : {false, true}) {
        LivingWorld::Host host(catalog, plan);
        const auto access = host.grant(host.entity(1));
        const auto action = host.action(access, host.entity(2)).value();
        ASSERT_TRUE(host.queue(access, action, host.fence()));
        if (schema) {
            ASSERT_TRUE(host.set_schema(2));
        } else {
            auto marin = host.actor(access, host.entity(2)).value();
            ++marin.ref.generation;
            ASSERT_TRUE(host.edit_actor(marin));
        }
        const auto receipt = host.settle(access, action);
        ASSERT_TRUE(receipt);
        EXPECT_EQ(receipt->state, LivingWorld::Disposition::Refused);
        EXPECT_EQ(receipt->reason, LivingWorld::Reason::Stale);
        EXPECT_EQ(host.actor(access, host.entity(1))->reserve, 10);
        EXPECT_EQ(host.actor(access, host.entity(2))->health, 12);
    }
}
