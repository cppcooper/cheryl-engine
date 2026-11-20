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

TEST(Storyactor, independent_agreement_and_release_use_real_actor_paths) {
    auto family = StoryFixture::family();
    auto host = Story::ActorCoordinator::create(std::move(family), {1}).value();
    const auto debtor = host->open(StoryFixture::sera).value();
    const auto creditor = host->open(StoryFixture::liora).value();
    const auto person = host->identify(debtor, StoryFixture::liora, Story::Text::make("Contact").value()).value();
    const auto object = host->identify(debtor, StoryFixture::box, Story::Text::make("Object").value()).value();
    auto execute = [&](const Story::ActorEndpoint& endpoint, std::uint64_t key, const Story::ActorIntent& intent) {
        const auto offer = host->offer(endpoint, intent, {}).value();
        if (!endpoint.submit({key}, choice(offer)) || !host->progress(endpoint)) {
            throw std::runtime_error("actor operation did not complete");
        }
        return endpoint.lookup({key}).value();
    };
    Story::ActorIntent intention;
    intention.kind = Story::IntentKind::OfferPromise;
    intention.person = person;
    intention.object = object;
    intention.due = {10};
    const auto offered = execute(debtor, 1, intention);
    ASSERT_TRUE(offered.promise);
    const Story::CommitmentId raw{StoryFixture::timeline, 1};
    EXPECT_EQ(host->owner().commitment(raw)->state, Story::CommitmentState::Offered);
    ASSERT_TRUE(host->refresh(creditor));
    const auto known = creditor.view()->accounts[0].fact.promise;
    intention = {};
    intention.kind = Story::IntentKind::DecidePromise;
    intention.promise = known;
    intention.accept = true;
    execute(creditor, 1, intention);
    EXPECT_EQ(host->owner().commitment(raw)->state, Story::CommitmentState::Pending);
    ASSERT_TRUE(host->refresh(debtor));
    intention = {};
    intention.kind = Story::IntentKind::RequestRelease;
    intention.promise = offered.promise;
    execute(debtor, 2, intention);
    EXPECT_EQ(host->owner().commitment(raw)->state, Story::CommitmentState::Pending);
    EXPECT_TRUE(host->owner().commitment(raw)->release_requested);
    ASSERT_TRUE(host->refresh(creditor));
    intention = {};
    intention.kind = Story::IntentKind::DecideRelease;
    intention.promise = known;
    intention.accept = false;
    execute(creditor, 2, intention);
    EXPECT_EQ(host->owner().commitment(raw)->state, Story::CommitmentState::Pending);
    EXPECT_FALSE(host->owner().commitment(raw)->release_requested);
    ASSERT_TRUE(host->refresh(debtor));
    intention = {};
    intention.kind = Story::IntentKind::RequestRelease;
    intention.promise = offered.promise;
    execute(debtor, 3, intention);
    ASSERT_TRUE(host->refresh(creditor));
    intention = {};
    intention.kind = Story::IntentKind::DecideRelease;
    intention.promise = known;
    intention.accept = true;
    execute(creditor, 3, intention);
    EXPECT_EQ(host->owner().commitment(raw)->state, Story::CommitmentState::Released);
    ASSERT_TRUE(host->refresh(debtor));
    EXPECT_EQ(debtor.view()->accounts[5].fact.kind, Story::ActorFactKind::ReleaseAccepted);
}

TEST(Storyactor, P07_P12_acquisition_and_work_exhaustion_do_not_publish_a_response) {
    for (const bool acquisition_limit : {true, false}) {
        Story::Limits limits;
        if (acquisition_limit) { limits.acquisitions = 4; }
        else { limits.work_per_action = 1; }
        auto world = response_world(false, limits);
        const auto offer = response_offer(world);
        ASSERT_TRUE(world.endpoint.submit({1}, choice(offer)));
        const auto before = world.endpoint.view().value();
        const auto fault = world.host->progress(world.endpoint);
        ASSERT_FALSE(fault);
        EXPECT_EQ(fault.error().error, acquisition_limit ? Story::Error::Capacity : Story::Error::WorkLimit);
        EXPECT_FALSE(fault.error().after_adoption);
        EXPECT_EQ(world.host->owner().episode_count(), 2);
        EXPECT_EQ(world.host->owner().acquisition_count(), 4);
        EXPECT_EQ(world.endpoint.view().value(), before);
        EXPECT_EQ(world.endpoint.lookup({1})->outcome, Story::ActorOutcome::Pending);
    }
}

TEST(Storyactor, P13_real_projection_budget_failure_retains_adopted_success) {
    Story::Limits limits;
    limits.output_bytes = 2 * sizeof(Story::Knowledge);
    auto world = response_world(false, limits);
    const auto body = choice(response_offer(world));
    ASSERT_TRUE(world.endpoint.submit({1}, body));
    const auto failure = world.host->progress(world.endpoint);
    ASSERT_FALSE(failure);
    EXPECT_EQ(failure.error().error, Story::Error::Capacity);
    EXPECT_TRUE(failure.error().after_adoption);
    EXPECT_EQ(world.host->owner().episode_count(), 3);
    EXPECT_EQ(world.endpoint.view().error(), Story::ActorError::Unavailable);
    const auto preparations = world.host->owner().counters().preparations;
    EXPECT_EQ(world.endpoint.submit({1}, body)->outcome, Story::ActorOutcome::Pending);
    EXPECT_FALSE(world.host->progress(world.endpoint));
    EXPECT_EQ(world.host->owner().episode_count(), 3);
    EXPECT_EQ(world.host->owner().counters().preparations, preparations);
}

TEST(Storyactor, actor_consent_and_exact_transfer_fulfill_only_the_agreed_object) {
    auto world = response_world();
    Story::ActorIntent consent;
    consent.kind = Story::IntentKind::ConsentTransfer;
    consent.person = world.contact;
    consent.object = world.object;
    const auto invitation = world.host->offer(world.endpoint, consent, {}).value();
    ASSERT_TRUE(world.endpoint.submit({1}, choice(invitation)));
    EXPECT_EQ(world.host->owner().object(StoryFixture::box)->accepted_recipient, Story::PersonId{});
    ASSERT_TRUE(world.host->progress(world.endpoint));
    EXPECT_EQ(world.host->owner().object(StoryFixture::box)->accepted_recipient, StoryFixture::liora);
    const auto giver = world.host->open(StoryFixture::sera).value();
    const auto recipient = world.host->identify(giver, StoryFixture::liora, {}).value();
    const auto object = world.host->identify(giver, StoryFixture::box, {}).value();
    Story::ActorIntent transfer;
    transfer.kind = Story::IntentKind::Transfer;
    transfer.person = recipient;
    transfer.object = object;
    const auto offered = world.host->offer(giver, transfer, {}).value();
    ASSERT_TRUE(giver.submit({1}, choice(offered)));
    ASSERT_TRUE(world.host->progress(giver));
    EXPECT_EQ(world.host->owner().object(StoryFixture::box)->holder, StoryFixture::liora);
    EXPECT_EQ(world.host->owner().commitment({StoryFixture::timeline, 1})->state, Story::CommitmentState::Fulfilled);
    EXPECT_EQ(giver.lookup({1})->outcome, Story::ActorOutcome::Completed);
    EXPECT_FALSE(giver.lookup({1})->promise);
    ASSERT_TRUE(world.host->refresh(world.endpoint));
    EXPECT_EQ(world.endpoint.view()->accounts[3].fact.kind, Story::ActorFactKind::Transferred);
}

TEST(Storyactor, P07_full_due_queue_is_not_an_actor_refusal) {
    Story::Limits limits;
    limits.queue = 1;
    auto world = response_world(false, limits);
    const auto debtor = world.host->open(StoryFixture::sera).value();
    Story::ActorIntent intent;
    intent.kind = Story::IntentKind::OfferPromise;
    intent.person = world.host->identify(debtor, StoryFixture::liora, {}).value();
    intent.object = world.host->identify(debtor, StoryFixture::box, {}).value();
    intent.due = {10};
    const auto offer = world.host->offer(debtor, intent, {}).value();
    ASSERT_TRUE(debtor.submit({1}, choice(offer)));
    const auto failure = world.host->progress(debtor);
    ASSERT_FALSE(failure);
    EXPECT_EQ(failure.error().error, Story::Error::Capacity);
    EXPECT_FALSE(failure.error().after_adoption);
    EXPECT_EQ(debtor.lookup({1})->outcome, Story::ActorOutcome::Pending);
    EXPECT_EQ(world.host->owner().episode_count(), 2);
}
