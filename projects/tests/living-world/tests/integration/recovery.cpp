#include <consumer/fixture.h>
#include <recovery/paired.h>
#include <tests/host/access.h>
#include <tests/integration/recovery-access.h>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace {
    namespace LivingWorld = CE::LivingWorld;
    struct RecoveryFixture : testing::Test {
        ::Magic::Catalog catalog = LivingWorld::fixture_catalog();
        ::Magic::PlanHandle plan = LivingWorld::fixture_plan(catalog);
        LivingWorld::Host host{catalog, plan};
        LivingWorld::Host::Access player = host.grant(host.entity(1));
        LivingWorld::Scene scene{host, player, host.entity(2)};
    };
}

TEST_F(RecoveryFixture, offered_pair_renews_every_live_capability) {
    const auto old_offer = scene.display().offer.value();
    const auto cursor = host.read(host.grant(host.entity(3)))->next;
    const auto captured = LivingWorld::Recovery::capture(host, scene);
    ASSERT_TRUE(captured);
    const auto action = LivingWorld::RecoveryTestAccess::action(*captured);
    auto first = LivingWorld::Recovery::restore(*captured, host, player);
    auto second = LivingWorld::Recovery::restore(*captured, host, player);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    auto& restored = first->host();
    auto& restored_scene = first->scene();
    const auto access = restored.grant(restored.entity(1));
    EXPECT_NE(restored.fence(), host.fence());
    EXPECT_NE(restored.fence(), second->host().fence());
    EXPECT_EQ(restored.actor(player, restored.entity(2)).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(restored_scene.request(old_offer).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(restored_scene.request(second->scene().display().offer.value()).error(),
              LivingWorld::Error::Unauthorized);
    EXPECT_EQ(restored.queue(access, action, host.fence()).error(), LivingWorld::Error::StaleFence);
    EXPECT_EQ(restored.read(restored.grant(restored.entity(3)), cursor).error(), LivingWorld::Error::Stale);
    const auto first_cursor = restored.read(restored.grant(restored.entity(3)))->next;
    EXPECT_EQ(second->host().read(second->host().grant(second->host().entity(3)), first_cursor).error(),
              LivingWorld::Error::Stale);
    ASSERT_TRUE(restored_scene.request(restored_scene.display().offer.value()));
    ASSERT_EQ(restored_scene.process()->state, LivingWorld::Disposition::Applied);
    EXPECT_EQ(restored.actor(access, restored.entity(2))->health, 20);
    EXPECT_EQ(restored.actor(access, restored.entity(1))->reserve, 7);
    EXPECT_EQ(scene.state(), LivingWorld::SceneState::Offered);
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 10);
}

TEST_F(RecoveryFixture, waiting_applied_restores_receipt_not_contact) {
    const auto offer = scene.display().offer.value();
    ASSERT_TRUE(scene.request(offer));
    const auto applied = scene.process();
    ASSERT_TRUE(applied);
    ASSERT_EQ(applied->state, LivingWorld::Disposition::Applied);
    auto captured = LivingWorld::Recovery::capture(host, scene);
    ASSERT_TRUE(captured);
    const auto action = LivingWorld::RecoveryTestAccess::action(*captured);
    auto recovered = LivingWorld::Recovery::restore(*captured, host, player);
    ASSERT_TRUE(recovered);
    auto& restored = recovered->host();
    auto& continuation = recovered->scene();
    const auto access = restored.grant(restored.entity(1));
    auto garo = restored.actor(access, restored.entity(4)).value();
    garo.observes = true;
    ASSERT_TRUE(restored.edit_actor(garo));
    EXPECT_EQ(continuation.state(), LivingWorld::SceneState::Waiting);
    EXPECT_EQ(continuation.request(offer).error(), LivingWorld::Error::Unauthorized);
    const auto failed = continuation.refresh([](std::string_view) { throw std::runtime_error("display"); });
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), LivingWorld::Error::Preparation);
    EXPECT_EQ(continuation.state(), LivingWorld::SceneState::Waiting);
    ASSERT_TRUE(continuation.refresh());
    EXPECT_EQ(continuation.state(), LivingWorld::SceneState::Helped);
    EXPECT_EQ(continuation.process().value(), *applied);
    EXPECT_EQ(restored.queue(access, action, host.fence()).value(), *applied);
    auto changed = action;
    ++changed.body.quote.price;
    EXPECT_EQ(restored.lookup(access, changed).error(), LivingWorld::Error::Conflict);
    auto unknown = action;
    ++unknown.key.sequence;
    EXPECT_EQ(restored.lookup(access, unknown).error(), LivingWorld::Error::Missing);
    EXPECT_EQ(restored.queue(access, unknown, restored.fence()).error(), LivingWorld::Error::InvalidBody);
    EXPECT_EQ(restored.deliver().adopted, 1);
    const auto wren = restored.read(restored.grant(restored.entity(3))).value();
    ASSERT_EQ(wren.size, 1);
    EXPECT_EQ(restored.read(restored.grant(restored.entity(4)))->size, 0);
    EXPECT_EQ(restored.deliver().adopted, 0);
    auto consumed = LivingWorld::Recovery::capture(restored, continuation);
    ASSERT_TRUE(consumed);
    auto again = LivingWorld::Recovery::restore(*consumed, restored, access);
    ASSERT_TRUE(again);
    EXPECT_EQ(again->scene().state(), LivingWorld::SceneState::Helped);
    EXPECT_EQ(again->scene().process().value(), *applied);
    EXPECT_EQ(again->host().deliver().adopted, 0);
    const auto retained = again->host().read(again->host().grant(again->host().entity(3))).value();
    ASSERT_EQ(retained.size, 1);
    EXPECT_EQ(retained.wound_closures[0], wren.wound_closures[0]);
    const auto new_player = again->host().grant(again->host().entity(1));
    EXPECT_EQ(again->host().actor(new_player, again->host().entity(2))->health, 20);
    EXPECT_EQ(again->host().actor(new_player, again->host().entity(1))->reserve, 7);
}

TEST_F(RecoveryFixture, captured_baseline_is_not_the_offers_old_reads) {
    ASSERT_TRUE(host.set_price(4));
    auto stale_offer = LivingWorld::Recovery::capture(host, scene);
    ASSERT_TRUE(stale_offer);
    auto recovered = LivingWorld::Recovery::restore(*stale_offer, host, player);
    ASSERT_TRUE(recovered);
    EXPECT_FALSE(recovered->scene().display().offer);
    EXPECT_EQ(recovered->host().actor(recovered->host().grant(recovered->host().entity(1)),
                                    recovered->host().entity(1))->reserve, 10);
    LivingWorld::Scene current(host, player, host.entity(2));
    ASSERT_TRUE(current.request(current.display().offer.value()));
    ASSERT_EQ(current.process()->state, LivingWorld::Disposition::Applied);
    ASSERT_TRUE(host.set_schema(2));
    auto terminal = LivingWorld::Recovery::capture(host, current);
    ASSERT_TRUE(terminal);
    auto resumed = LivingWorld::Recovery::restore(*terminal, host, player);
    ASSERT_TRUE(resumed);
    ASSERT_TRUE(resumed->scene().refresh());
    EXPECT_EQ(resumed->scene().state(), LivingWorld::SceneState::Helped);
    EXPECT_EQ(resumed->host().actor(resumed->host().grant(resumed->host().entity(1)),
                                  resumed->host().entity(1))->reserve, 6);
}

TEST_F(RecoveryFixture, raw_queued_and_active_presentation_defer) {
    auto cut = LivingWorld::Recovery::capture(host, scene);
    ASSERT_TRUE(cut);
    const auto action = LivingWorld::RecoveryTestAccess::action(*cut);
    const auto raw_action = host.action(player, host.entity(2)).value();
    {
        auto raw = LivingWorld::HostTestAccess::reserve(host, player, raw_action);
        ASSERT_TRUE(raw);
        EXPECT_EQ(LivingWorld::Recovery::capture(host, scene).error(), LivingWorld::Error::Busy);
        EXPECT_EQ(LivingWorld::Recovery::restore(*cut, host, player).error(), LivingWorld::Error::Busy);
    }
    ASSERT_TRUE(scene.request(scene.display().offer.value()));
    EXPECT_EQ(LivingWorld::Recovery::capture(host, scene).error(), LivingWorld::Error::Busy);
    EXPECT_EQ(host.lookup(player, action)->state, LivingWorld::Disposition::Queued);
    ASSERT_TRUE(scene.process());
    ASSERT_TRUE(scene.refresh([&](std::string_view) {
        EXPECT_EQ(LivingWorld::Recovery::capture(host, scene).error(), LivingWorld::Error::Busy);
        EXPECT_EQ(LivingWorld::Recovery::restore(*cut, host, player).error(), LivingWorld::Error::Busy);
    }));
    EXPECT_EQ(host.actor(player, host.entity(1))->reserve, 7);
}

TEST_F(RecoveryFixture, current_access_is_not_a_saved_permission) {
    ASSERT_TRUE(scene.request(scene.display().offer.value()));
    ASSERT_TRUE(scene.process());
    auto cut = LivingWorld::Recovery::capture(host, scene);
    ASSERT_TRUE(cut);
    auto wren = host.actor(player, host.entity(3)).value();
    wren.access = false;
    ASSERT_TRUE(host.edit_actor(wren));
    auto recovered = LivingWorld::Recovery::restore(*cut, host, player);
    ASSERT_TRUE(recovered);
    auto& restored = recovered->host();
    const auto witness = restored.grant(restored.entity(3));
    EXPECT_EQ(restored.read(witness).error(), LivingWorld::Error::Unauthorized);
    EXPECT_TRUE(restored.deliver().withheld);
    EXPECT_EQ(restored.deliver().adopted, 0);
    const auto access = restored.grant(restored.entity(1));
    auto authorized_wren = restored.actor(access, restored.entity(3)).value();
    authorized_wren.access = true;
    ASSERT_TRUE(restored.edit_actor(authorized_wren));
    EXPECT_EQ(restored.deliver().adopted, 1);
    auto player_actor = host.actor(player, host.entity(1)).value();
    player_actor.access = false;
    ASSERT_TRUE(host.edit_actor(player_actor));
    EXPECT_EQ(LivingWorld::Recovery::restore(*cut, host, player).error(), LivingWorld::Error::Unauthorized);
    EXPECT_EQ(LivingWorld::Recovery::capture(host, scene).error(), LivingWorld::Error::Unauthorized);
}

TEST_F(RecoveryFixture, refused_terminal_and_cancelled_history_survive) {
    const auto cancelled = host.action(player, host.entity(2)).value();
    {
        auto raw = LivingWorld::HostTestAccess::reserve(host, player, cancelled);
        ASSERT_TRUE(raw);
    }
    auto marin = host.actor(player, host.entity(2)).value();
    marin.willing = false;
    ASSERT_TRUE(host.edit_actor(marin));
    LivingWorld::Scene refusal(host, player, host.entity(2));
    ASSERT_TRUE(refusal.request(refusal.display().offer.value()));
    const auto refused = refusal.process();
    ASSERT_TRUE(refused);
    ASSERT_EQ(refused->reason, LivingWorld::Reason::Contact);
    for (bool terminal : {false, true}) {
        if (terminal)
            ASSERT_TRUE(refusal.refresh());
        auto cut = LivingWorld::Recovery::capture(host, refusal);
        ASSERT_TRUE(cut);
        auto recovered = LivingWorld::Recovery::restore(*cut, host, player);
        ASSERT_TRUE(recovered);
        auto& restored = recovered->host();
        const auto access = restored.grant(restored.entity(1));
        EXPECT_EQ(restored.queue(access, cancelled, restored.fence()).error(), LivingWorld::Error::InvalidBody);
        auto changed = cancelled;
        ++changed.body.quote.price;
        EXPECT_EQ(restored.queue(access, changed, restored.fence()).error(), LivingWorld::Error::Conflict);
        EXPECT_EQ(recovered->scene().process().value(), *refused);
        ASSERT_TRUE(recovered->scene().refresh());
        EXPECT_EQ(recovered->scene().state(), LivingWorld::SceneState::Refused);
        EXPECT_EQ(restored.deliver().adopted, 0);
        EXPECT_EQ(restored.actor(access, restored.entity(1))->reserve, 10);
        EXPECT_EQ(restored.actor(access, restored.entity(2))->health, 12);
        const auto next = restored.action(access, restored.entity(2));
        ASSERT_TRUE(next);
        EXPECT_EQ(next->key.sequence, LivingWorld::RecoveryTestAccess::issued(*cut) + 1);
    }
}

TEST_F(RecoveryFixture, corrupt_pair_refuses_without_touching_original) {
    ASSERT_TRUE(scene.request(scene.display().offer.value()));
    ASSERT_TRUE(scene.process());
    auto cut = LivingWorld::Recovery::capture(host, scene);
    ASSERT_TRUE(cut);
    const auto before = std::make_unique<LivingWorld::Detail::Image>(LivingWorld::HostTestAccess::image(host));
    const auto reject = [&](auto mutate, LivingWorld::Error expected) {
        auto corrupt = LivingWorld::RecoveryTestAccess::corrupt(*cut, mutate);
        const auto restored = LivingWorld::Recovery::restore(corrupt, host, player);
        ASSERT_FALSE(restored);
        EXPECT_EQ(restored.error(), expected);
        const auto& after = LivingWorld::HostTestAccess::image(host);
        EXPECT_EQ(after.revision, before->revision);
        EXPECT_EQ(after.actors, before->actors);
        EXPECT_EQ(scene.state(), LivingWorld::SceneState::Waiting);
    };
    reject([](auto& saved) { ++saved.schema; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.scene_cut.reset(); }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { ++saved.scene_scope.branch; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { ++saved.baseline; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.image.actor_count = 9; }, LivingWorld::Error::Capacity);
    reject([](auto& saved) { saved.image.record_count = 17; }, LivingWorld::Error::Capacity);
    reject([](auto& saved) { saved.image.work_count = 17; }, LivingWorld::Error::Capacity);
    reject([](auto& saved) { saved.image.acquired_count = 33; }, LivingWorld::Error::Capacity);
    reject([](auto& saved) { ++saved.image.actors[1].ref.generation; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.image.actors[1].ref = saved.image.actors[0].ref; },
           LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { --saved.issued; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { ++saved.image.binding.schema; }, LivingWorld::Error::Stale);
    reject([](auto& saved) { ++saved.image.records[0].action.body.plan.revision; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.image.records[0].receipt.state = LivingWorld::Disposition::Queued; },
           LivingWorld::Error::Busy);
    reject([](auto& saved) { saved.image.records[0].cancelled = true; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.image.records[0].receipt.reason = LivingWorld::Reason::Contact; },
           LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.image.records[0].witness_count = 5; }, LivingWorld::Error::Capacity);
    reject([](auto& saved) { ++saved.image.records[0].action.body.quote.id; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.image.records[0].action.body.reads.stamps[1].owner = 0; },
           LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.image.work[0].consumed = true; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.image.work_count = 0; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.image.work[0].witness = saved.image.actors[3].ref; },
           LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { ++saved.action.body.quote.price; }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { ++saved.navigation.definition_revision; }, LivingWorld::Error::Navigation);
    reject([](auto& saved) { ++saved.navigation.schema; }, LivingWorld::Error::Navigation);
    reject([](auto& saved) { saved.navigation.node.value = "offer"; }, LivingWorld::Error::Navigation);
    reject([](auto& saved) { saved.plan.reset(); }, LivingWorld::Error::InvalidBody);
    reject([](auto& saved) { saved.catalog.operations[0].max_quantity -= 1; }, LivingWorld::Error::InvalidBody);
    LivingWorld::Host foreign(catalog, plan);
    EXPECT_EQ(LivingWorld::Recovery::restore(*cut, foreign, foreign.grant(foreign.entity(1))).error(),
              LivingWorld::Error::WrongScope);
    EXPECT_EQ(LivingWorld::Recovery::capture(foreign, scene).error(), LivingWorld::Error::WrongScope);
    auto replaced = host.actor(player, host.entity(4)).value();
    ++replaced.ref.generation;
    ASSERT_TRUE(host.edit_actor(replaced));
    EXPECT_EQ(LivingWorld::Recovery::restore(*cut, host, player).error(), LivingWorld::Error::Stale);
}

TEST_F(RecoveryFixture, consumed_work_and_scoped_ids_are_validated) {
    ASSERT_TRUE(scene.request(scene.display().offer.value()));
    ASSERT_TRUE(scene.process());
    ASSERT_EQ(host.deliver().adopted, 1);
    auto cut = LivingWorld::Recovery::capture(host, scene);
    ASSERT_TRUE(cut);
    const auto reject = [&](auto mutate) {
        auto corrupt = LivingWorld::RecoveryTestAccess::corrupt(*cut, mutate);
        const auto result = LivingWorld::Recovery::restore(corrupt, host, player);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), LivingWorld::Error::InvalidBody);
    };
    reject([](auto& saved) { saved.image.acquired_count = 0; });
    reject([](auto& saved) { ++saved.image.acquired[0].local_id; });
    reject([](auto& saved) { ++saved.image.acquired[0].subject.generation; });
    reject([](auto& saved) { saved.image.acquired[0].acquired_at = saved.image.tick + 1; });
    reject([](auto& saved) { saved.image.work[0].consumed = false; });
    EXPECT_EQ(host.read(host.grant(host.entity(3)))->size, 1);
    EXPECT_EQ(host.deliver().adopted, 0);
}

TEST_F(RecoveryFixture, capacity_and_sequence_exhaustion_stay_bounded) {
    for (std::size_t index = 0; index < 16; ++index) {
        const auto action = host.action(player, host.entity(2)).value();
        auto raw = LivingWorld::HostTestAccess::reserve(host, player, action);
        ASSERT_TRUE(raw);
    }
    auto cut = LivingWorld::Recovery::capture(host, scene);
    ASSERT_TRUE(cut);
    auto recovered = LivingWorld::Recovery::restore(*cut, host, player);
    ASSERT_TRUE(recovered);
    auto& restored = recovered->host();
    EXPECT_EQ(LivingWorld::HostTestAccess::image(restored).record_count, 16);
    EXPECT_EQ(recovered->scene().request(recovered->scene().display().offer.value()).error(),
              LivingWorld::Error::Capacity);
    auto exhausted = LivingWorld::RecoveryTestAccess::corrupt(*cut, [](auto& saved) {
        saved.issued = std::numeric_limits<std::uint64_t>::max();
    });
    auto last = LivingWorld::Recovery::restore(exhausted, host, player);
    ASSERT_TRUE(last);
    EXPECT_EQ(last->host().action(last->host().grant(last->host().entity(1)), last->host().entity(2)).error(),
              LivingWorld::Error::Exhausted);
    EXPECT_EQ(last->scene().state(), LivingWorld::SceneState::Offered);
}
