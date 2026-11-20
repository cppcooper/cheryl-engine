#include "fixture.h"
#include <gtest/gtest.h>
#include <stdexcept>

using namespace ActorFixture;

TEST(Storyactor, P01_raw_known_account_time_counterexample) {
    auto plain = StoryFixture::family();
    auto hidden = StoryFixture::family();
    StoryFixture::promise(*plain);
    StoryFixture::promise(*hidden);
    hidden_transfer(*hidden);
    std::array<Story::Knowledge, 8> left{}, right{};
    ASSERT_EQ(plain->knowledge(plain->actor(StoryFixture::liora), left).value(), 2);
    ASSERT_EQ(hidden->knowledge(hidden->actor(StoryFixture::liora), right).value(), 2);
    EXPECT_EQ(left, right);
    const auto request = StoryFixture::request(2, 3, Story::Respond{StoryFixture::sera, 2, -1});
    EXPECT_TRUE(plain->apply(plain->actor(StoryFixture::liora), request));
    EXPECT_EQ(hidden->apply(hidden->actor(StoryFixture::liora), request).error, Story::Error::Time);
}

TEST(Storyactor, P02_matched_public_clock_uses_the_real_owner) {
    auto plain = response_world();
    auto hidden = response_world(true);
    const auto left_offer = response_offer(plain);
    const auto right_offer = response_offer(hidden);
    EXPECT_EQ(canonical(plain.endpoint.view().value()), canonical(hidden.endpoint.view().value()));
    ASSERT_TRUE(plain.endpoint.submit({7}, choice(left_offer)));
    ASSERT_TRUE(hidden.endpoint.submit({7}, choice(right_offer)));
    ASSERT_TRUE(plain.host->progress(plain.endpoint));
    ASSERT_TRUE(hidden.host->progress(hidden.endpoint));
    EXPECT_EQ(plain.endpoint.lookup({7})->outcome, Story::ActorOutcome::Completed);
    EXPECT_EQ(canonical(plain.endpoint.lookup({7}).value()), canonical(hidden.endpoint.lookup({7}).value()));
    EXPECT_EQ(canonical(plain.endpoint.view().value()), canonical(hidden.endpoint.view().value()));
    EXPECT_EQ(plain.host->owner().episode_count(), 3);
    EXPECT_EQ(hidden.host->owner().episode_count(), 5);
    EXPECT_EQ(plain.endpoint.view()->accounts[2].acquired, (Story::StoryTime{5}));
}

TEST(Storyactor, P03_P05_same_public_time_hidden_history_preserves_offers) {
    auto world = response_world(true);
    const auto offer = response_offer(world);
    const auto before = world.endpoint.view().value();
    ASSERT_TRUE(scheduled(world.host->owner(), StoryFixture::sera, {5},
        Story::ConsentTransfer{StoryFixture::substitute, StoryFixture::oren, 3}));
    ASSERT_TRUE(world.host->refresh(world.endpoint));
    EXPECT_EQ(response_offer(world), offer);
    EXPECT_EQ(world.endpoint.view().value(), before);
    ASSERT_TRUE(world.endpoint.submit({9}, choice(offer)));
    ASSERT_TRUE(world.host->progress(world.endpoint));
    EXPECT_EQ(world.endpoint.view()->now, (Story::StoryTime{5}));
}

TEST(Storyactor, P04_nested_promise_and_outcome_refs_ignore_hidden_allocation) {
    auto setup = [](bool hidden) {
        StoryFixture::Inputs inputs;
        inputs.objects[1].holder = StoryFixture::oren;
        auto family = Story::Family::create(inputs.seed());
        if (!family) { throw std::runtime_error("seed prerequisite"); }
        if (hidden && !scheduled(**family, StoryFixture::oren, {0},
            Story::OfferPromise{StoryFixture::liora, StoryFixture::substitute, {20}, 1})) {
            throw std::runtime_error("hidden offer prerequisite");
        }
        auto host = Story::ActorCoordinator::create(std::move(*family), {5});
        if (!host) { throw std::runtime_error("host prerequisite"); }
        auto endpoint = (*host)->open(StoryFixture::sera).value();
        auto person = (*host)->identify(endpoint, StoryFixture::liora, Story::Text::make("Contact").value()).value();
        auto object = (*host)->identify(endpoint, StoryFixture::box, Story::Text::make("Object").value()).value();
        return World{std::move(*host), endpoint, person, object};
    };
    auto left = setup(false);
    auto right = setup(true);
    for (auto* world : {&left, &right}) {
        Story::ActorIntent intent;
        intent.kind = Story::IntentKind::OfferPromise;
        intent.person = world->contact;
        intent.object = world->object;
        intent.due = {10};
        const auto offer = world->host->offer(world->endpoint, intent, Story::Text::make("Promise").value()).value();
        ASSERT_TRUE(world->endpoint.submit({1}, choice(offer)));
        ASSERT_TRUE(world->host->progress(world->endpoint));
        const auto view = world->endpoint.view().value();
        ASSERT_EQ(view.account_count, 1);
        EXPECT_EQ(view.accounts[0].fact.subject, view.self);
        EXPECT_EQ(view.accounts[0].fact.other, world->contact);
        EXPECT_EQ(view.accounts[0].fact.object, world->object);
        EXPECT_EQ(view.accounts[0].fact.promise, world->endpoint.lookup({1})->promise);
        EXPECT_EQ(view.accounts[0].fact.due, (Story::StoryTime{10}));
    }
    EXPECT_EQ(canonical(left.endpoint.view().value()), canonical(right.endpoint.view().value()));
    EXPECT_EQ(canonical(left.endpoint.lookup({1}).value()), canonical(right.endpoint.lookup({1}).value()));
    EXPECT_EQ(left.host->owner().episode_count(), 1);
    EXPECT_EQ(right.host->owner().episode_count(), 2);
}

TEST(Storyactor, P11_tell_is_attributed_and_mentions_are_not_identity) {
    auto world = response_world();
    const auto receiver = world.host->open(StoryFixture::oren).value();
    const auto unrelated = world.host->open(StoryFixture::sera).value();
    const auto before = unrelated.view().value();
    const auto recipient = world.host->identify(world.endpoint, StoryFixture::oren, Story::Text::make("Contact").value()).value();
    Story::ActorIntent intent;
    intent.kind = Story::IntentKind::Tell;
    intent.person = recipient;
    intent.account = world.endpoint.view()->accounts[1].reference;
    const auto offer = world.host->offer(world.endpoint, intent, Story::Text::make("Tell account").value()).value();
    ASSERT_TRUE(world.endpoint.submit({1}, choice(offer)));
    ASSERT_TRUE(world.host->progress(world.endpoint));
    ASSERT_TRUE(world.host->refresh(receiver));
    const auto view = receiver.view().value();
    ASSERT_EQ(view.account_count, 1);
    EXPECT_EQ(view.accounts[0].evidence, Story::ActorEvidence::Attributed);
    EXPECT_NE(view.accounts[0].fact.other, view.accounts[0].attribution);
    const auto identified = world.host->identify(receiver, StoryFixture::liora, Story::Text::make("Sender").value()).value();
    EXPECT_EQ(identified, view.accounts[0].attribution);
    EXPECT_NE(identified, view.accounts[0].fact.other);
    Story::ActorIntent forged;
    forged.kind = Story::IntentKind::Transfer;
    forged.person = view.accounts[0].fact.subject;
    forged.object = view.accounts[0].fact.object;
    const auto rejected = world.host->offer(receiver, forged, {});
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error().error, Story::Error::Terms);
    const auto acquisitions = world.host->owner().acquisition_count();
    ASSERT_TRUE(world.endpoint.submit({1}, choice(offer)));
    ASSERT_TRUE(world.host->progress(world.endpoint));
    ASSERT_TRUE(world.host->refresh(receiver));
    ASSERT_TRUE(world.host->refresh(unrelated));
    EXPECT_EQ(receiver.view()->account_count, 1);
    EXPECT_EQ(world.host->owner().acquisition_count(), acquisitions);
    EXPECT_EQ(unrelated.view().value(), before);
}

TEST(Storyactor, P14_paging_is_local_and_refs_are_stable) {
    auto plain = response_world();
    auto hidden = response_world(true);
    const auto first = plain.endpoint.view(0, 1).value();
    const auto second = plain.endpoint.view(first.cursor, 1).value();
    EXPECT_EQ(first.total_accounts, 2);
    EXPECT_EQ(first.cursor, 1);
    EXPECT_EQ(second.cursor, 2);
    EXPECT_EQ(first.accounts[0].fact.promise, second.accounts[0].fact.promise);
    EXPECT_EQ(canonical(first), canonical(hidden.endpoint.view(0, 1).value()));
    EXPECT_EQ(canonical(second), canonical(hidden.endpoint.view(1, 1).value()));
    EXPECT_EQ(plain.endpoint.view(2, 1)->account_count, 0);
    EXPECT_EQ(plain.endpoint.view(3, 1).error(), Story::ActorError::Invalid);
    EXPECT_EQ(plain.endpoint.view(0, 0).error(), Story::ActorError::Invalid);
}

TEST(Storyactor, life_partnership_facts_preserve_visible_aliases) {
    auto family = StoryFixture::family();
    ASSERT_TRUE(StoryFixture::agreement(*family, StoryFixture::sera, StoryFixture::liora, 1));
    auto host = Story::ActorCoordinator::create(std::move(family), {5}).value();
    const auto endpoint = host->open(StoryFixture::liora).value();
    const auto view = endpoint.view().value();
    ASSERT_EQ(view.account_count, 2);
    EXPECT_TRUE(view.accounts[0].fact.partnership);
    EXPECT_EQ(view.accounts[0].fact.partnership, view.accounts[1].fact.partnership);
    EXPECT_EQ(view.accounts[0].fact.other, view.self);
    EXPECT_EQ(view.accounts[1].fact.other, view.self);
    EXPECT_FALSE(view.accounts[0].fact.promise);
}
