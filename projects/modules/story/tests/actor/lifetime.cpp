#include "fixture.h"
#include <gtest/gtest.h>
#include <stdexcept>

using namespace ActorFixture;

TEST(Storyactor, P15_owned_snapshot_survives_but_endpoint_expires) {
    Story::ActorEndpoint expired;
    Story::ActorView snapshot;
    Story::ActorChoice old_choice;
    {
        auto world = response_world();
        old_choice = choice(response_offer(world));
        snapshot = world.endpoint.view().value();
        expired = world.endpoint;
    }
    EXPECT_EQ(snapshot.account_count, 2);
    EXPECT_EQ(snapshot.accounts[1].fact.kind, Story::ActorFactKind::PromiseAccepted);
    EXPECT_EQ(expired.view().error(), Story::ActorError::Expired);
    EXPECT_EQ(expired.lookup({1}).error(), Story::ActorError::Expired);
    EXPECT_EQ(expired.submit({1}, old_choice).error(), Story::ActorError::Expired);
    auto replacement = response_world();
    response_offer(replacement);
    EXPECT_EQ(replacement.endpoint.submit({1}, old_choice).error(), Story::ActorError::Invalid);
    EXPECT_FALSE(replacement.host->refresh(expired));
}

TEST(Storyactor, P16_actor_only_consumer_drives_real_owner) {
    auto world = response_world();
    response_offer(world);
    const auto trace = ActorConsumer::choose_response(world.endpoint, {23});
    ASSERT_TRUE(trace);
    EXPECT_EQ(trace->pending.outcome, Story::ActorOutcome::Pending);
    const auto before = world.host->owner().episode_count();
    ASSERT_TRUE(world.host->progress(world.endpoint));
    const auto observed = ActorConsumer::observe(world.endpoint, {23});
    ASSERT_TRUE(observed);
    EXPECT_TRUE(observed->complete);
    EXPECT_EQ(observed->disposition.outcome, Story::ActorOutcome::Completed);
    EXPECT_EQ(world.host->owner().episode_count(), before + 1);
}

TEST(Storyactor, P17_private_response_ignores_unseen_referent_loss) {
    auto setup = [](int loss) {
        auto family = StoryFixture::family();
        StoryFixture::promise(*family);
        if (!scheduled(*family, StoryFixture::liora, {3}, Story::ConsentTransfer{StoryFixture::box, StoryFixture::sera, 1}) ||
            !scheduled(*family, StoryFixture::sera, {4}, Story::Transfer{StoryFixture::box, StoryFixture::liora, 2})) {
            throw std::runtime_error("settled promise prerequisite");
        }
        if (loss == 1 && !family->apply(family->life(), StoryFixture::life_request(1, 5,
            Story::Death{StoryFixture::sera, family->person(StoryFixture::sera)->version}))) {
            throw std::runtime_error("death prerequisite");
        }
        if (loss == 2 && !scheduled(*family, StoryFixture::sera, {5},
            Story::SetMembership{family->person(StoryFixture::sera)->version, false})) {
            throw std::runtime_error("departure prerequisite");
        }
        auto host = Story::ActorCoordinator::create(std::move(family), {6}).value();
        const auto endpoint = host->open(StoryFixture::liora).value();
        const auto contact = host->identify(endpoint, StoryFixture::sera, Story::Text::make("Known contact").value()).value();
        const auto object = host->identify(endpoint, StoryFixture::box, Story::Text::make("Known object").value()).value();
        return World{std::move(host), endpoint, contact, object};
    };
    auto living = setup(0);
    auto dead = setup(1);
    auto departed = setup(2);
    for (auto* world : {&living, &dead, &departed}) { response_offer(*world); }
    EXPECT_EQ(canonical(living.endpoint.view().value()), canonical(dead.endpoint.view().value()));
    EXPECT_EQ(canonical(living.endpoint.view().value()), canonical(departed.endpoint.view().value()));
    for (auto* world : {&living, &dead, &departed}) {
        const auto offer = world->endpoint.view()->offers[0];
        ASSERT_TRUE(world->endpoint.submit({1}, choice(offer)));
        ASSERT_TRUE(world->host->progress(world->endpoint));
        EXPECT_EQ(world->endpoint.lookup({1})->outcome, Story::ActorOutcome::Completed);
    }
    EXPECT_EQ(canonical(living.endpoint.view().value()), canonical(dead.endpoint.view().value()));
    EXPECT_EQ(canonical(living.endpoint.view().value()), canonical(departed.endpoint.view().value()));
    EXPECT_TRUE(dead.host->owner().person(StoryFixture::sera)->died.has_value());
    EXPECT_FALSE(departed.host->owner().person(StoryFixture::sera)->member);
}

TEST(Storyactor, finite_local_ledger_and_projection_limits) {
    auto world = response_world();
    const auto body = choice(response_offer(world));
    for (std::uint64_t attempt = 1; attempt <= 32; ++attempt) {
        ASSERT_TRUE(world.endpoint.submit({attempt}, body));
        ASSERT_TRUE(world.host->progress(world.endpoint));
    }
    EXPECT_EQ(world.endpoint.submit({33}, body).error(), Story::ActorError::Limit);
    ASSERT_TRUE(world.endpoint.retire_through({32}));
    for (std::uint64_t attempt = 33; attempt <= 62; ++attempt) {
        ASSERT_TRUE(world.endpoint.submit({attempt}, body));
        ASSERT_TRUE(world.host->progress(world.endpoint));
        ASSERT_TRUE(world.endpoint.retire_through({attempt}));
    }
    EXPECT_EQ(world.endpoint.view()->total_accounts, 64);
    const auto before = world.host->owner().episode_count();
    ASSERT_TRUE(world.endpoint.submit({63}, body));
    const auto full = world.host->progress(world.endpoint);
    ASSERT_FALSE(full);
    EXPECT_EQ(full.error().error, Story::Error::Capacity);
    EXPECT_FALSE(full.error().after_adoption);
    EXPECT_EQ(world.host->owner().episode_count(), before);
    EXPECT_EQ(world.endpoint.lookup({63})->outcome, Story::ActorOutcome::Pending);
}

TEST(Storyactor, trusted_future_frontier_and_attempt_exhaustion) {
    auto world = response_world(true);
    const auto body = choice(response_offer(world));
    ASSERT_TRUE(scheduled(world.host->owner(), StoryFixture::sera, {6},
        Story::ConsentTransfer{StoryFixture::substitute, StoryFixture::oren, 3}));
    ASSERT_TRUE(world.endpoint.submit({1}, body));
    const auto future = world.host->progress(world.endpoint);
    ASSERT_FALSE(future);
    EXPECT_EQ(future.error().error, Story::Error::Time);
    EXPECT_EQ(world.endpoint.lookup({1})->outcome, Story::ActorOutcome::Pending);
    ASSERT_TRUE(world.host->advance({6}));
    const auto capability = world.host->owner().actor(StoryFixture::liora);
    ASSERT_EQ(world.host->owner().retire_through(capability, std::numeric_limits<std::uint64_t>::max()), Story::Error::None);
    const auto exhausted = world.host->progress(world.endpoint);
    ASSERT_FALSE(exhausted);
    EXPECT_EQ(exhausted.error().error, Story::Error::Capacity);
    EXPECT_EQ(world.endpoint.lookup({1})->outcome, Story::ActorOutcome::Pending);
}
