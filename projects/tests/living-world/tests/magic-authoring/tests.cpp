#include <consumer/fixture.h>
#include <magic/authoring/authoring.h>
#include <recovery/paired.h>
#include <tests/host/access.h>
#include <tests/integration/recovery-access.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace {
    namespace LivingWorld = CE::LivingWorld;
    struct MagicAuthoring : testing::Test {
        ::Magic::Catalog catalog = LivingWorld::fixture_catalog();
        ::Magic::PlanHandle plan = LivingWorld::fixture_plan(catalog);
        LivingWorld::Host host{catalog, plan};
        LivingWorld::Host::Access access = host.grant(host.entity(1));
        LivingWorld::MagicAuthoring authoring{host, access};

        std::string quantity(unsigned value) const {
            std::string source(plan->source());
            const auto offset = source.find("12 hp");
            if (offset == std::string::npos)
                throw std::runtime_error("S1 numeric spelling changed");
            source.replace(offset, 5, std::to_string(value) + " hp");
            return source;
        }
    };
}

TEST_F(MagicAuthoring, literal_meanings_and_effects) {
    for (const auto amount : {12u, 5u}) {
        LivingWorld::Host owner(catalog, plan);
        const auto player = owner.grant(owner.entity(1));
        LivingWorld::MagicAuthoring drafts(owner, player);
        const auto source = quantity(amount);
        const auto draft = drafts.edit(source);
        ASSERT_TRUE(draft);
        ASSERT_EQ((*draft)->state, LivingWorld::DraftState::Accepted);
        ASSERT_TRUE((*draft)->accepted);
        const auto inspected = owner.inspect(player, *(*draft)->accepted);
        ASSERT_TRUE(inspected);
        EXPECT_EQ((*inspected)->source(), source);
        ASSERT_EQ((*inspected)->meaning().effects.size(), 1u);
        EXPECT_EQ((*inspected)->meaning().effects[0].quantity, amount);
        ASSERT_EQ((*inspected)->provenance().size(), 1u);
        EXPECT_EQ((*inspected)->provenance()[0].rule, ::Magic::Rule::MendHeal);
        const auto span = (*inspected)->provenance()[0].clause;
        EXPECT_LE(span.offset + span.length, source.size());
        EXPECT_NE(source.substr(span.offset, span.length).find(std::to_string(amount)), std::string::npos);
        const auto action = drafts.action(owner.entity(2));
        ASSERT_TRUE(action);
        ASSERT_TRUE(owner.queue(player, *action, owner.fence()));
        const auto receipt = owner.settle(player, *action);
        ASSERT_TRUE(receipt);
        EXPECT_EQ(receipt->state, LivingWorld::Disposition::Applied);
        EXPECT_EQ(receipt->gain, amount == 12 ? 8u : 5u);
        EXPECT_EQ(owner.actor(player, owner.entity(2))->health, amount == 12 ? 20u : 17u);
        EXPECT_EQ(owner.actor(player, owner.entity(1))->reserve, 7u);
    }
}

TEST_F(MagicAuthoring, bad_edit_never_selects_last_good) {
    const auto good = authoring.edit(quantity(12)).value();
    ASSERT_TRUE(good->accepted);
    for (const auto source : {"cercle de feu", "feu-ring", "unknown construction", ""}) {
        const auto draft = authoring.edit(source);
        ASSERT_TRUE(draft);
        EXPECT_EQ((*draft)->state, LivingWorld::DraftState::Invalid);
        EXPECT_FALSE((*draft)->accepted);
        ASSERT_TRUE((*draft)->failure->diagnostic);
        const auto& diagnostic = *(*draft)->failure->diagnostic;
        EXPECT_LE(diagnostic.span.offset, (*draft)->source.size());
        EXPECT_FALSE(authoring.quote(host.entity(2)));
        EXPECT_EQ(authoring.action(host.entity(2)).error(), LivingWorld::Error::InvalidBody);
    }
    EXPECT_EQ(LivingWorld::HostTestAccess::issued(host), 0u);
    EXPECT_EQ(host.actor(access, host.entity(1))->reserve, 10u);
    const auto explicit_prior = host.action(access, host.entity(2), *good->accepted);
    ASSERT_TRUE(explicit_prior);
    ASSERT_TRUE(host.queue(access, *explicit_prior, host.fence()));
    EXPECT_EQ(host.settle(access, *explicit_prior)->gain, 8u);
}

TEST_F(MagicAuthoring, compiler_success_is_not_contact_support) {
    const auto good = authoring.edit(quantity(5));
    ASSERT_TRUE(good);
    ASSERT_EQ((*good)->state, LivingWorld::DraftState::Accepted);
    ASSERT_TRUE((*good)->accepted);
    ASSERT_TRUE(authoring.quote(host.entity(2)));
    const auto revision = LivingWorld::HostTestAccess::image(host).revision;
    ASSERT_EQ(LivingWorld::HostTestAccess::image(host).plan_count, 2u);
    const auto payer = host.actor(access, host.entity(1));
    const auto target = host.actor(access, host.entity(2));
    ASSERT_TRUE(payer);
    ASSERT_TRUE(target);
    EXPECT_EQ(payer->reserve, 10u);
    EXPECT_EQ(target->health, 12u);

    std::string source(plan->source());
    const auto delivery = source.find("via contact");
    ASSERT_NE(delivery, std::string::npos);
    source.replace(delivery, 11, "via ray");
    const auto compiler = ::Magic::compile(source, catalog);
    ASSERT_TRUE(compiler);
    EXPECT_EQ((*compiler)->meaning().delivery, ::Magic::Delivery::Ray);
    EXPECT_EQ((*compiler)->meaning().onset_ticks, 0u);
    EXPECT_FALSE(::Magic::Contact::supports(*compiler));
    const auto accepted = host.accept(access, source);
    ASSERT_FALSE(accepted);
    EXPECT_EQ(accepted.error().error, LivingWorld::Error::InvalidBody);
    EXPECT_TRUE(accepted.error().unsupported);
    EXPECT_FALSE(accepted.error().diagnostic);

    const auto draft = authoring.edit(source);
    ASSERT_TRUE(draft);
    EXPECT_EQ((*draft)->state, LivingWorld::DraftState::Unsupported);
    ASSERT_TRUE((*draft)->failure);
    EXPECT_EQ((*draft)->failure->error, LivingWorld::Error::InvalidBody);
    EXPECT_TRUE((*draft)->failure->unsupported);
    EXPECT_FALSE((*draft)->failure->diagnostic);
    EXPECT_FALSE((*draft)->accepted);
    EXPECT_EQ(authoring.latest(), *draft);
    const auto quote = authoring.quote(host.entity(2));
    ASSERT_FALSE(quote);
    EXPECT_EQ(quote.error(), LivingWorld::Error::InvalidBody);
    const auto action = authoring.action(host.entity(2));
    ASSERT_FALSE(action);
    EXPECT_EQ(action.error(), LivingWorld::Error::InvalidBody);
    ASSERT_TRUE(host.inspect(access, *(*good)->accepted));

    const auto& image = LivingWorld::HostTestAccess::image(host);
    EXPECT_EQ(image.plan_count, 2u);
    EXPECT_EQ(image.revision, revision);
    EXPECT_EQ(LivingWorld::HostTestAccess::issued(host), 0u);
    EXPECT_EQ(image.record_count, 0u);
    EXPECT_EQ(image.work_count, 0u);
    EXPECT_EQ(image.acquired_count, 0u);
    const auto payer_after = host.actor(access, host.entity(1));
    const auto target_after = host.actor(access, host.entity(2));
    ASSERT_TRUE(payer_after);
    ASSERT_TRUE(target_after);
    EXPECT_EQ(*payer_after, *payer);
    EXPECT_EQ(*target_after, *target);
}

TEST_F(MagicAuthoring, inspection_and_quote_are_pure) {
    const auto draft = authoring.edit(quantity(5)).value();
    const auto revision = LivingWorld::HostTestAccess::image(host).revision;
    const auto issued = LivingWorld::HostTestAccess::issued(host);
    for (unsigned repeat = 0; repeat < 4; ++repeat) {
        ASSERT_TRUE(host.inspect(access, *draft->accepted));
        const auto quote = authoring.quote(host.entity(2));
        ASSERT_TRUE(quote);
        EXPECT_EQ(quote->body().quote.price, 3u);
        EXPECT_EQ(quote->body().caster, host.entity(1));
        EXPECT_EQ(quote->body().recipient, host.entity(2));
    }
    const auto& image = LivingWorld::HostTestAccess::image(host);
    EXPECT_EQ(image.revision, revision);
    EXPECT_EQ(LivingWorld::HostTestAccess::issued(host), issued);
    EXPECT_EQ(image.record_count, 0u);
    EXPECT_EQ(image.work_count, 0u);
    EXPECT_EQ(image.acquired_count, 0u);
    EXPECT_EQ(image.actors[0].reserve, 10u);
    EXPECT_EQ(image.actors[1].health, 12u);
}

TEST_F(MagicAuthoring, quote_expires_on_semantic_dependencies) {
    for (unsigned dependency = 0; dependency < 4; ++dependency) {
        LivingWorld::Host owner(catalog, plan);
        const auto player = owner.grant(owner.entity(1));
        const auto accepted = owner.accept(player, quantity(5)).value();
        const auto quote = owner.quote(player, accepted, owner.entity(2)).value();
        if (dependency == 0) {
            ASSERT_TRUE(owner.set_price(4));
        } else if (dependency == 1) {
            ASSERT_TRUE(owner.set_schema(2));
        } else {
            auto actor = owner.actor(player, owner.entity(dependency == 2 ? 1 : 2)).value();
            if (dependency == 2)
                actor.can_cast = false;
            else
                actor.willing = false;
            ASSERT_TRUE(owner.edit_actor(actor));
        }
        EXPECT_EQ(owner.action(player, quote).error(), LivingWorld::Error::Stale);
        EXPECT_EQ(LivingWorld::HostTestAccess::issued(owner), 0u);
        EXPECT_EQ(owner.actor(player, owner.entity(1))->reserve, 10u);
        EXPECT_EQ(owner.actor(player, owner.entity(2))->health, 12u);
        EXPECT_EQ(LivingWorld::HostTestAccess::image(owner).record_count, 0u);
    }
}

TEST_F(MagicAuthoring, acceptance_does_not_stale_an_exact_reservation) {
    const auto accepted = host.accept(access, quantity(5)).value();
    const auto action = host.action(access, host.entity(2), accepted).value();
    ASSERT_TRUE(host.queue(access, action, host.fence()));
    ASSERT_TRUE(host.accept(access, quantity(7)));
    EXPECT_EQ(host.settle(access, action)->gain, 5u);
    EXPECT_EQ(host.actor(access, host.entity(1))->reserve, 7u);
}

TEST_F(MagicAuthoring, exact_history_survives_selection_and_bad_edits) {
    const auto first = authoring.edit(quantity(12)).value();
    const auto older = authoring.action(host.entity(2)).value();
    ASSERT_TRUE(host.queue(access, older, host.fence()));
    ASSERT_EQ(host.settle(access, older)->gain, 8u);
    const auto second = authoring.edit(quantity(5)).value();
    const auto newer = authoring.action(host.entity(4)).value();
    ASSERT_TRUE(host.queue(access, newer, host.fence()));
    ASSERT_EQ(host.settle(access, newer)->gain, 5u);
    EXPECT_NE(older.body.plan, newer.body.plan);
    ASSERT_TRUE(authoring.edit("feu-ring"));
    EXPECT_FALSE(authoring.action(host.entity(2)));
    ASSERT_TRUE(host.set_price(9));
    ASSERT_TRUE(host.set_schema(2));
    EXPECT_EQ(host.queue(access, older, {99, 99})->gain, 8u);
    EXPECT_EQ(host.settle(access, newer)->gain, 5u);
    auto changed = older;
    changed.body.plan = newer.body.plan;
    EXPECT_EQ(host.queue(access, changed, host.fence()).error(), LivingWorld::Error::Conflict);
    EXPECT_EQ(host.actor(access, host.entity(1))->reserve, 4u);
    EXPECT_EQ(host.inspect(access, *first->accepted).value()->meaning().effects[0].quantity, 12u);
    EXPECT_EQ(host.inspect(access, *second->accepted).value()->meaning().effects[0].quantity, 5u);
}

TEST_F(MagicAuthoring, explicit_self_cast_and_legacy_default) {
    const auto draft = authoring.edit(quantity(5)).value();
    const auto self = host.action(access, host.entity(1), *draft->accepted).value();
    ASSERT_TRUE(host.queue(access, self, host.fence()));
    EXPECT_EQ(host.settle(access, self)->gain, 5u);
    EXPECT_EQ(host.actor(access, host.entity(1))->health, 17u);
    EXPECT_EQ(host.actor(access, host.entity(1))->reserve, 7u);
    const auto legacy = host.action(access, host.entity(2)).value();
    ASSERT_TRUE(host.queue(access, legacy, host.fence()));
    EXPECT_EQ(host.settle(access, legacy)->gain, 8u);
    EXPECT_EQ(host.actor(access, host.entity(1))->reserve, 4u);
}

TEST_F(MagicAuthoring, handles_are_not_foreign_authority) {
    const auto accepted = host.accept(access, quantity(5)).value();
    const auto quote = host.quote(access, accepted, host.entity(2)).value();
    LivingWorld::Host other(catalog, plan);
    const auto player = other.grant(other.entity(1));
    EXPECT_EQ(other.inspect(player, accepted).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(other.quote(player, accepted, other.entity(2)).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(other.action(player, quote).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(host.inspect({}, accepted).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(host.inspect(host.grant(host.entity(3)), accepted).error(), LivingWorld::Error::Unauthorized);
}

TEST_F(MagicAuthoring, delivery_is_exact_after_edit_and_lost_wake) {
    ASSERT_TRUE(authoring.edit(quantity(12)));
    const auto first = authoring.action(host.entity(2)).value();
    ASSERT_TRUE(host.queue(access, first, host.fence()));
    ASSERT_TRUE(host.settle(access, first));
    ASSERT_TRUE(authoring.edit(quantity(5)));
    const auto second = authoring.action(host.entity(4)).value();
    ASSERT_TRUE(host.queue(access, second, host.fence()));
    ASSERT_TRUE(host.settle(access, second));
    auto witness = host.actor(access, host.entity(3)).value();
    witness.access = false;
    ASSERT_TRUE(host.edit_actor(witness));
    EXPECT_TRUE(host.deliver().withheld);
    EXPECT_FALSE(LivingWorld::Scene::notify([] { throw std::runtime_error("lost wake"); }));
    EXPECT_EQ(host.delivery(access, first)->outstanding, 1u);
    EXPECT_EQ(host.delivery(access, second)->outstanding, 1u);
    EXPECT_EQ(host.queue(access, first, {99, 99})->gain, 8u);
    EXPECT_EQ(host.actor(access, host.entity(1))->reserve, 4u);
    witness.access = true;
    ASSERT_TRUE(host.edit_actor(witness));
    EXPECT_EQ(host.deliver().adopted, 2u);
    EXPECT_EQ(host.delivery(access, first)->acquired, 1u);
    EXPECT_EQ(host.delivery(access, second)->acquired, 1u);
    EXPECT_EQ(host.deliver().adopted, 0u);
}

TEST_F(MagicAuthoring, paired_cut_keeps_each_exact_plan) {
    LivingWorld::Scene scene(host, access, host.entity(2));
    const auto twelve = authoring.edit(quantity(12)).value();
    const auto first = authoring.action(host.entity(2)).value();
    ASSERT_TRUE(host.queue(access, first, host.fence()));
    ASSERT_TRUE(host.settle(access, first));
    const auto five = authoring.edit(quantity(5)).value();
    const auto second = authoring.action(host.entity(4)).value();
    ASSERT_TRUE(host.queue(access, second, host.fence()));
    EXPECT_EQ(LivingWorld::Recovery::capture(host, scene).error(), LivingWorld::Error::Busy);
    ASSERT_EQ(host.settle(access, second)->gain, 5u);
    ASSERT_TRUE(host.set_price(4));
    ASSERT_TRUE(host.set_schema(2));
    auto cut = LivingWorld::Recovery::capture(host, scene);
    ASSERT_TRUE(cut);
    ASSERT_TRUE(authoring.edit(quantity(7)));
    auto recovered = LivingWorld::Recovery::restore(*cut, host, access);
    ASSERT_TRUE(recovered);
    auto& restored = recovered->host();
    const auto player = restored.grant(restored.entity(1));
    EXPECT_EQ(restored.lookup(player, first)->gain, 8u);
    EXPECT_EQ(restored.lookup(player, second)->gain, 5u);
    EXPECT_EQ(restored.queue(player, first, host.fence())->gain, 8u);
    EXPECT_EQ(restored.actor(player, restored.entity(1))->reserve, 4u);
    EXPECT_EQ(restored.actor(player, restored.entity(2))->health, 20u);
    EXPECT_EQ(restored.actor(player, restored.entity(4))->health, 17u);
    EXPECT_EQ(restored.inspect(player, *twelve->accepted).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(restored.inspect(player, *five->accepted).error(), LivingWorld::Error::Unauthorized);
    EXPECT_FALSE(recovered->scene().display().offer);
    const auto renewed = restored.accept(player, quantity(5));
    ASSERT_TRUE(renewed);
    EXPECT_EQ(restored.inspect(player, *renewed).value()->meaning().effects[0].quantity, 5u);
    const auto current_quote = restored.quote(player, *renewed, restored.entity(3));
    ASSERT_TRUE(current_quote);
    EXPECT_EQ(current_quote->body().quote.price, 4u);
    EXPECT_EQ(current_quote->body().plan.schema, 2u);
    EXPECT_EQ(restored.deliver().adopted, 2u);
}

TEST_F(MagicAuthoring, malformed_historical_plan_and_gain_refuse) {
    LivingWorld::Scene scene(host, access, host.entity(2));
    ASSERT_TRUE(authoring.edit(quantity(5)));
    const auto action = authoring.action(host.entity(2)).value();
    ASSERT_TRUE(host.queue(access, action, host.fence()));
    ASSERT_EQ(host.settle(access, action)->gain, 5u);
    const auto cut = LivingWorld::Recovery::capture(host, scene);
    ASSERT_TRUE(cut);
    const auto reject = [&](auto mutate) {
        const auto corrupt = LivingWorld::RecoveryTestAccess::corrupt(*cut, mutate);
        const auto result = LivingWorld::Recovery::restore(corrupt, host, access);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), LivingWorld::Error::InvalidBody);
    };
    reject([](auto& saved) { saved.image.plans[1].plan.reset(); });
    reject([](auto& saved) { saved.image.plan_count = 1; });
    reject([](auto& saved) { saved.image.plans[1].plan = saved.plan; });
    reject([](auto& saved) { ++saved.image.plans[1].binding.catalog; });
    reject([](auto& saved) { saved.image.records[0].receipt.gain = 4; });
    reject([](auto& saved) { --saved.image.records[0].applied->resource; });
    reject([](auto& saved) { ++saved.image.records[0].applied->before.price; });
    reject([](auto& saved) { saved.image.records[0].applied.reset(); });
    EXPECT_EQ(host.lookup(access, action)->gain, 5u);
    EXPECT_EQ(host.actor(access, host.entity(1))->reserve, 7u);
}

TEST_F(MagicAuthoring, capacities_do_not_install_partial_plans_or_fallback) {
    const auto prior = authoring.edit(quantity(12)).value();
    for (unsigned count = 1; count < 8; ++count)
        ASSERT_TRUE(authoring.edit(quantity(12)));
    const auto revision = LivingWorld::HostTestAccess::image(host).revision;
    EXPECT_EQ(authoring.edit(quantity(5)).error(), LivingWorld::Error::Capacity);
    EXPECT_FALSE(authoring.latest());
    EXPECT_FALSE(authoring.action(host.entity(2)));
    EXPECT_EQ(LivingWorld::HostTestAccess::image(host).revision, revision);
    ASSERT_TRUE(host.inspect(access, *prior->accepted));
    for (unsigned amount = 1; amount < 8; ++amount)
        ASSERT_TRUE(host.accept(access, quantity(amount)));
    const auto full_revision = LivingWorld::HostTestAccess::image(host).revision;
    const auto overflow = host.accept(access, quantity(8));
    ASSERT_FALSE(overflow);
    EXPECT_EQ(overflow.error().error, LivingWorld::Error::Capacity);
    EXPECT_EQ(LivingWorld::HostTestAccess::image(host).plan_count, 8u);
    EXPECT_EQ(LivingWorld::HostTestAccess::image(host).revision, full_revision);
    EXPECT_EQ(LivingWorld::HostTestAccess::issued(host), 0u);
}

TEST_F(MagicAuthoring, source_and_compiler_work_bounds_refuse_without_fallback) {
    const auto good = authoring.edit(quantity(5)).value();
    const std::string too_large(8193, ' ');
    EXPECT_EQ(authoring.edit(too_large).error(), LivingWorld::Error::Capacity);
    EXPECT_FALSE(authoring.latest());
    EXPECT_FALSE(authoring.action(host.entity(2)));
    ASSERT_TRUE(host.inspect(access, *good->accepted));
    const auto direct = host.accept(access, too_large);
    ASSERT_FALSE(direct);
    ASSERT_TRUE(direct.error().diagnostic);
    EXPECT_EQ(direct.error().diagnostic->code, ::Magic::Code::SourceLimit);
    EXPECT_EQ(direct.error().diagnostic->span.offset, 8192u);
    EXPECT_EQ(LivingWorld::HostTestAccess::image(host).plan_count, 2u);
    EXPECT_EQ(LivingWorld::HostTestAccess::issued(host), 0u);
}
