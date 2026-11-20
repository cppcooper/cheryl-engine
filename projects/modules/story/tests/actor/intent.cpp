#include "fixture.h"
#include <gtest/gtest.h>
#include <stdexcept>

using namespace ActorFixture;

TEST(Storyactor, P06_foreign_local_values_fail_before_owner_work) {
    auto world = response_world();
    auto foreign = response_world();
    const auto offer = response_offer(world);
    const auto other = response_offer(foreign);
    const auto preparations = world.host->owner().counters().preparations;
    auto invalid = choice(offer);
    invalid.intent.person = foreign.contact;
    EXPECT_EQ(world.endpoint.submit({1}, invalid).error(), Story::ActorError::Invalid);
    EXPECT_EQ(world.endpoint.submit({1}, choice(other)).error(), Story::ActorError::Invalid);
    invalid = choice(offer);
    invalid.offer = {};
    EXPECT_EQ(world.endpoint.submit({1}, invalid).error(), Story::ActorError::Invalid);
    const auto wrong_host = foreign.host->progress(world.endpoint);
    ASSERT_FALSE(wrong_host);
    EXPECT_EQ(wrong_host.error().error, Story::Error::Authority);
    EXPECT_EQ(world.host->owner().counters().preparations, preparations);
    EXPECT_EQ(world.endpoint.lookup({1}).error(), Story::ActorError::Unknown);
    ASSERT_TRUE(world.endpoint.submit({1}, choice(offer)));
}

TEST(Storyactor, P07_P12_raw_exhaustion_remains_trusted_and_atomic) {
    Story::Limits limits;
    limits.episodes = 2;
    auto world = response_world(false, limits);
    const auto offer = response_offer(world);
    const auto before = world.endpoint.view().value();
    const auto acquisitions = world.host->owner().acquisition_count();
    ASSERT_TRUE(world.endpoint.submit({1}, choice(offer)));
    const auto refused = world.host->progress(world.endpoint);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().error, Story::Error::Capacity);
    EXPECT_FALSE(refused.error().after_adoption);
    EXPECT_EQ(world.endpoint.lookup({1})->outcome, Story::ActorOutcome::Pending);
    EXPECT_EQ(world.endpoint.view().value(), before);
    EXPECT_EQ(world.host->owner().episode_count(), 2);
    EXPECT_EQ(world.host->owner().acquisition_count(), acquisitions);
    const auto preparations = world.host->owner().counters().preparations;
    EXPECT_FALSE(world.host->progress(world.endpoint));
    EXPECT_EQ(world.host->owner().counters().preparations, preparations);
}

TEST(Storyactor, P08_local_outstanding_backpressure_and_retirement) {
    auto world = response_world();
    const auto offer = response_offer(world);
    const auto body = choice(offer);
    const auto pending = world.endpoint.submit({1}, body).value();
    EXPECT_EQ(world.endpoint.submit({2}, body).error(), Story::ActorError::Busy);
    EXPECT_EQ(world.endpoint.submit({1}, body).value(), pending);
    EXPECT_EQ(world.endpoint.retire_through({1}).error(), Story::ActorError::Busy);
    ASSERT_TRUE(world.host->progress(world.endpoint));
    ASSERT_TRUE(world.endpoint.retire_through({1}));
    EXPECT_EQ(world.endpoint.submit({1}, body).error(), Story::ActorError::Expired);
    ASSERT_TRUE(world.endpoint.submit({2}, body));
}

TEST(Storyactor, P09_exact_retry_precedes_clock_offer_and_owner_changes) {
    auto world = response_world();
    const auto offer = response_offer(world);
    const auto body = choice(offer);
    ASSERT_TRUE(world.endpoint.submit({11}, body));
    ASSERT_TRUE(world.host->progress(world.endpoint));
    const auto receipt = world.endpoint.lookup({11}).value();
    ASSERT_EQ(receipt.outcome, Story::ActorOutcome::Completed);
    ASSERT_TRUE(world.host->advance({6}));
    ASSERT_TRUE(scheduled(world.host->owner(), StoryFixture::liora, {6}, Story::Respond{StoryFixture::sera, 2, 1}));
    ASSERT_TRUE(world.host->refresh(world.endpoint));
    const auto episodes = world.host->owner().episode_count();
    EXPECT_EQ(world.endpoint.submit({11}, body).value(), receipt);
    auto changed = body;
    changed.intent.attitude_delta = 1;
    EXPECT_EQ(world.endpoint.submit({11}, changed).error(), Story::ActorError::Conflict);
    ASSERT_TRUE(world.host->progress(world.endpoint));
    EXPECT_EQ(world.host->owner().episode_count(), episodes);
    ASSERT_TRUE(world.endpoint.submit({12}, body));
    ASSERT_TRUE(world.host->progress(world.endpoint));
    EXPECT_EQ(world.host->owner().episode_count(), episodes + 1);
    EXPECT_EQ(world.endpoint.lookup({12})->outcome, Story::ActorOutcome::Completed);
}

TEST(Storyactor, P10_handover_observes_noncompletion_only_after_attempt) {
    auto setup = [](bool death) {
        auto family = StoryFixture::family();
        if (!scheduled(*family, StoryFixture::liora, {1}, Story::ConsentTransfer{StoryFixture::box, StoryFixture::sera, 1})) {
            throw std::runtime_error("consent prerequisite");
        }
        if (death && !family->apply(family->life(), StoryFixture::life_request(1, 2,
            Story::Death{StoryFixture::liora, family->person(StoryFixture::liora)->version}))) {
            throw std::runtime_error("death prerequisite");
        }
        auto host = Story::ActorCoordinator::create(std::move(family), {5}).value();
        const auto endpoint = host->open(StoryFixture::sera).value();
        const auto person = host->identify(endpoint, StoryFixture::liora, Story::Text::make("Recipient").value()).value();
        const auto object = host->identify(endpoint, StoryFixture::box, Story::Text::make("Object").value()).value();
        return World{std::move(host), endpoint, person, object};
    };
    auto plain = setup(false);
    auto hidden = setup(true);
    for (auto* world : {&plain, &hidden}) {
        Story::ActorIntent intent;
        intent.kind = Story::IntentKind::Transfer;
        intent.person = world->contact;
        intent.object = world->object;
        const auto offer = world->host->offer(world->endpoint, intent, Story::Text::make("Hand over").value()).value();
        ASSERT_TRUE(world->endpoint.submit({1}, choice(offer)));
    }
    EXPECT_EQ(canonical(plain.endpoint.view().value()), canonical(hidden.endpoint.view().value()));
    ASSERT_TRUE(plain.host->progress(plain.endpoint));
    ASSERT_TRUE(hidden.host->progress(hidden.endpoint));
    EXPECT_EQ(plain.endpoint.lookup({1})->outcome, Story::ActorOutcome::Completed);
    EXPECT_EQ(hidden.endpoint.lookup({1})->outcome, Story::ActorOutcome::NotCompleted);
    EXPECT_EQ(hidden.host->owner().object(StoryFixture::box)->holder, StoryFixture::sera);
}

TEST(Storyactor, P13_post_adoption_delivery_failure_never_reapplies) {
    auto world = response_world();
    const auto offer = response_offer(world);
    ASSERT_TRUE(world.host->delivery(world.endpoint, false));
    ASSERT_TRUE(world.endpoint.submit({1}, choice(offer)));
    const auto failed = world.host->progress(world.endpoint);
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.error().after_adoption);
    const auto episodes = world.host->owner().episode_count();
    const auto preparations = world.host->owner().counters().preparations;
    EXPECT_EQ(world.endpoint.lookup({1})->outcome, Story::ActorOutcome::Pending);
    EXPECT_EQ(world.endpoint.submit({1}, choice(offer))->outcome, Story::ActorOutcome::Pending);
    EXPECT_FALSE(world.host->progress(world.endpoint));
    ASSERT_TRUE(world.host->delivery(world.endpoint, true));
    ASSERT_TRUE(world.host->progress(world.endpoint));
    EXPECT_EQ(world.endpoint.lookup({1})->outcome, Story::ActorOutcome::Completed);
    EXPECT_EQ(world.host->owner().episode_count(), episodes);
    EXPECT_EQ(world.host->owner().counters().preparations, preparations);
    EXPECT_EQ(world.endpoint.view()->account_count, 3);
}
