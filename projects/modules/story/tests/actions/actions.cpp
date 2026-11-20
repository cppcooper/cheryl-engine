#include <contract/fixture.h>
#include <gtest/gtest.h>
#include <vector>

using namespace StoryFixture;

TEST(Storyactions, offer_and_independent_decline) {
    auto owner = family();
    const auto offer = owner->apply(owner->actor(sera), request(1, 1, Story::OfferPromise{liora, box, {10}, 1}));
    ASSERT_TRUE(offer);
    EXPECT_EQ(owner->commitment(offer.promise)->state, Story::CommitmentState::Offered);
    const auto wrong = request(2, 2, Story::DecidePromise{offer.promise, 1, true});
    EXPECT_EQ(owner->apply(owner->actor(sera), wrong).error, Story::Error::Authority);
    const auto decline = request(1, 3, Story::DecidePromise{offer.promise, 1, false});
    ASSERT_TRUE(owner->apply(owner->actor(liora), decline));
    EXPECT_EQ(owner->commitment(offer.promise)->state, Story::CommitmentState::Declined);
}

TEST(Storyactions, stale_terms_and_missing_consent) {
    auto owner = family();
    const auto commitment = promise(*owner);
    const auto stale = request(2, 3, Story::DecidePromise{commitment, 1, true});
    EXPECT_EQ(owner->apply(owner->actor(liora), stale).error, Story::Error::Stale);
    const auto transfer = request(2, 4, Story::Transfer{box, liora, 1});
    EXPECT_EQ(owner->apply(owner->actor(sera), transfer).error, Story::Error::Consent);
    EXPECT_EQ(owner->object(box)->holder, sera);
    EXPECT_EQ(owner->episode_count(), 2);
}

TEST(Storyactions, substitute_not_fulfillment) {
    auto owner = family();
    const auto commitment = promise(*owner);
    ASSERT_TRUE(owner->apply(owner->actor(liora), request(2, 3, Story::ConsentTransfer{substitute, sera, 1})));
    ASSERT_TRUE(owner->apply(owner->actor(sera), request(2, 4, Story::Transfer{substitute, liora, 2})));
    EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Pending);
    EXPECT_EQ(owner->object(box)->holder, sera);
    EXPECT_EQ(owner->object(substitute)->holder, liora);
}

TEST(Storyactions, release_requires_creditor_decision) {
    auto owner = family();
    const auto commitment = promise(*owner);
    ASSERT_TRUE(owner->apply(owner->actor(sera), request(2, 3, Story::RequestRelease{commitment, 2})));
    EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Pending);
    ASSERT_TRUE(owner->apply(owner->actor(liora), request(2, 4, Story::DecideRelease{commitment, 3, false})));
    EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Pending);
    ASSERT_TRUE(owner->apply(owner->actor(sera), request(3, 5, Story::RequestRelease{commitment, 4})));
    ASSERT_TRUE(owner->apply(owner->actor(liora), request(3, 6, Story::DecideRelease{commitment, 5, true})));
    EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Released);
}

TEST(Storyactions, deadline_is_not_care_or_transfer) {
    auto owner = family();
    const auto commitment = promise(*owner);
    ASSERT_TRUE(owner->apply(owner->actor(liora), request(2, 11, Story::CheckDeadline{commitment, 2})));
    EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Breached);
    EXPECT_EQ(owner->object(box)->holder, sera);
}

TEST(Storyactions, retry_conflict_and_retirement) {
    auto owner = family();
    const auto access = owner->actor(sera);
    const auto offer = request(1, 1, Story::OfferPromise{liora, box, {10}, 1});
    const auto original = owner->apply(access, offer);
    ASSERT_TRUE(original);
    EXPECT_EQ(owner->apply(access, offer), original);
    auto changed = offer;
    std::get<Story::OfferPromise>(changed.action).creditor = oren;
    EXPECT_EQ(owner->apply(access, changed).error, Story::Error::Conflict);
    ASSERT_EQ(owner->retire_through(access, 1), Story::Error::None);
    EXPECT_EQ(owner->apply(access, offer).error, Story::Error::Expired);
    EXPECT_EQ(owner->episode_count(), 1);
}

TEST(Storyactions, foreign_capability_and_record) {
    auto owner = family();
    auto other = family();
    const auto offer = request(1, 1, Story::OfferPromise{liora, box, {10}, 1});
    EXPECT_EQ(owner->apply(other->actor(sera), offer).error, Story::Error::Authority);
    const auto foreign = request(1, 1, Story::OfferPromise{liora, {{42}, 71}, {10}, 1});
    EXPECT_EQ(owner->apply(owner->actor(sera), foreign).error, Story::Error::ForeignReference);
    EXPECT_EQ(owner->episode_count(), 0);
}

TEST(Storyactions, unknown_account_and_unobserved_incident) {
    auto owner = family();
    const auto commitment = promise(*owner);
    static_cast<void>(commitment);
    const auto tell = request(1, 3, Story::Tell{liora, 1});
    EXPECT_EQ(owner->apply(owner->actor(oren), tell).error, Story::Error::UnknownAccount);
    const auto perceive = request(2, 4, Story::Perceive{{timeline, 1}});
    EXPECT_EQ(owner->apply(owner->actor(oren), perceive).error, Story::Error::Ineligible);
}

TEST(Storyactions, acquisition_capacity_rolls_back_prepared_transfer) {
    Story::Limits limits;
    limits.acquisitions = 7;
    auto owner = family(limits);
    const auto commitment = promise(*owner);
    ASSERT_TRUE(owner->apply(owner->actor(liora), request(2, 3, Story::ConsentTransfer{box, sera, 1})));
    const auto transfer = request(2, 4, Story::Transfer{box, liora, 2});
    EXPECT_EQ(owner->apply(owner->actor(sera), transfer).error, Story::Error::Capacity);
    EXPECT_EQ(owner->object(box)->holder, sera);
    EXPECT_EQ(owner->object(box)->version, 2);
    EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Pending);
    EXPECT_EQ(owner->episode_count(), 3);
    EXPECT_EQ(owner->acquisition_count(), 6);
}

TEST(Storyactions, unrelated_activity_does_not_stale_object) {
    auto owner = family();
    const auto commitment = promise(*owner);
    ASSERT_TRUE(owner->apply(owner->actor(liora), request(2, 3, Story::ConsentTransfer{box, sera, 1})));
    ASSERT_TRUE(owner->apply(owner->actor(oren), request(1, 4, Story::ConsentTransfer{substitute, sera, 1})));
    ASSERT_TRUE(owner->apply(owner->actor(sera), request(2, 5, Story::Transfer{box, liora, 2})));
    EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Fulfilled);
}

TEST(Storyactions, outsider_cannot_probe_promise_versions_or_existence) {
    auto owner = family();
    const auto commitment = promise(*owner);
    const auto access = owner->actor(oren);
    const auto correct = request(1, 3, Story::DecidePromise{commitment, 2, true});
    const auto stale = request(2, 4, Story::DecidePromise{commitment, 1, true});
    const auto absent = request(3, 5, Story::DecidePromise{{timeline, 999}, 1, true});
    EXPECT_EQ(owner->apply(access, correct).error, Story::Error::Authority);
    EXPECT_EQ(owner->apply(access, stale).error, Story::Error::Authority);
    EXPECT_EQ(owner->apply(access, absent).error, Story::Error::Authority);
    EXPECT_EQ(owner->episode_count(), 2);
    EXPECT_EQ(owner->acquisition_count(), 4);
}

TEST(Storyactions, finite_receipts_preserve_retries_without_eviction) {
    Story::Limits limits;
    limits.receipts = 1;
    auto owner = family(limits);
    const auto access = owner->actor(sera);
    const auto offer = request(1, 1, Story::OfferPromise{liora, box, {10}, 1});
    const auto retained = owner->apply(access, offer);
    ASSERT_TRUE(retained);
    const auto another = request(2, 2, Story::OfferPromise{oren, substitute, {10}, 1});
    EXPECT_EQ(owner->apply(access, another).error, Story::Error::Capacity);
    EXPECT_EQ(owner->apply(access, offer), retained);
    EXPECT_EQ(owner->episode_count(), 1);
}

TEST(Storyactions, queue_saturation_preserves_all_accepted_promises) {
    Story::Limits limits;
    limits.queue = 1;
    auto owner = family(limits);
    const auto first = owner->apply(owner->actor(sera), request(1, 1, Story::OfferPromise{liora, box, {10}, 1}));
    ASSERT_TRUE(first);
    const auto second = request(2, 2, Story::OfferPromise{oren, substitute, {10}, 1});
    EXPECT_EQ(owner->apply(owner->actor(sera), second).error, Story::Error::Capacity);
    EXPECT_EQ(owner->commitment(first.promise)->state, Story::CommitmentState::Offered);
    EXPECT_EQ(owner->episode_count(), 1);
    EXPECT_EQ(owner->object(substitute)->holder, sera);
}

TEST(Storyactions, episode_and_output_saturation_are_explicit) {
    Story::Limits limits;
    limits.episodes = 1;
    limits.output_bytes = sizeof(Story::Knowledge) - 1;
    auto owner = family(limits);
    const auto offer = owner->apply(owner->actor(sera), request(1, 1, Story::OfferPromise{liora, box, {10}, 1}));
    ASSERT_TRUE(offer);
    const auto accept = request(1, 2, Story::DecidePromise{offer.promise, 1, true});
    EXPECT_EQ(owner->apply(owner->actor(liora), accept).error, Story::Error::Capacity);
    EXPECT_EQ(owner->commitment(offer.promise)->state, Story::CommitmentState::Offered);
    std::array<Story::Knowledge, 2> output{};
    output[0].local_sequence = 123;
    const auto projected = owner->knowledge(owner->actor(liora), output);
    ASSERT_FALSE(projected);
    EXPECT_EQ(projected.error(), Story::Error::Capacity);
    EXPECT_EQ(output[0].local_sequence, 123);
}

TEST(Storyactions, sparse_transfer_and_high_degree_contact_budget) {
    Inputs inputs;
    std::vector<Story::Person> people(inputs.people.begin(), inputs.people.end());
    std::vector<Story::Contact> contacts(inputs.contacts.begin(), inputs.contacts.end());
    for (std::uint64_t index = 0; index < 32; ++index) {
        auto incoming = inputs.people[2];
        incoming.id = {timeline, 1000 + index};
        incoming.origin = {timeline, incoming.id.value};
        incoming.address = {1, incoming.id.value};
        incoming.parents = {};
        incoming.lineage = Story::Lineage::Incoming;
        people.push_back(incoming);
        contacts.push_back({liora, incoming.id});
    }
    auto expanded = inputs.seed();
    expanded.people = people;
    expanded.contacts = contacts;
    const auto created = Story::Family::create(expanded);
    ASSERT_TRUE(created);
    auto small = family();
    std::array<std::uint64_t, 2> touched{};
    std::size_t sample = 0;
    for (auto* owner : {small.get(), created->get()}) {
        const auto commitment = promise(*owner);
        ASSERT_TRUE(owner->apply(owner->actor(liora), request(2, 3, Story::ConsentTransfer{box, sera, 1})));
        const auto before = owner->counters();
        ASSERT_TRUE(owner->apply(owner->actor(sera), request(2, 4, Story::Transfer{box, liora, 2})));
        const auto after = owner->counters();
        touched[sample++] = after.records_touched - before.records_touched;
        EXPECT_EQ(after.links_traversed - before.links_traversed, 1);
        EXPECT_EQ(after.validation_passes, 1);
        EXPECT_EQ(after.whole_state_copies, 0);
        EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Fulfilled);
    }
    EXPECT_EQ(touched[0], 9);
    EXPECT_EQ(touched[1], 9);
    const auto before_tell = (*created)->counters();
    ASSERT_TRUE((*created)->apply((*created)->actor(liora), request(3, 5, Story::Tell{oren, 4})));
    EXPECT_EQ((*created)->counters().links_traversed - before_tell.links_traversed, 33);

    Story::Limits limits;
    limits.work_per_action = 4;
    const auto bounded = Story::Family::create(expanded, limits);
    ASSERT_TRUE(bounded);
    const auto commitment = promise(**bounded);
    static_cast<void>(commitment);
    const auto tell = request(2, 3, Story::Tell{oren, 2});
    EXPECT_EQ((*bounded)->apply((*bounded)->actor(liora), tell).error, Story::Error::WorkLimit);
    EXPECT_EQ((*bounded)->episode_count(), 2);
    EXPECT_EQ((*bounded)->acquisition_count(), 4);
    EXPECT_EQ((*bounded)->counters().links_traversed, 4);
}

TEST(Storyactions, attributed_evidence_supports_bounded_local_response) {
    auto owner = family();
    const auto trace = baseline(*owner);
    ASSERT_TRUE(trace.told);
    const auto access = owner->actor(oren);
    const auto excessive = request(2, 7, Story::Respond{liora, 1, 6});
    EXPECT_EQ(owner->apply(access, excessive).error, Story::Error::Terms);
    const auto respond = request(3, 8, Story::Respond{liora, 1, -2});
    const auto accepted = owner->apply(access, respond);
    ASSERT_TRUE(accepted);
    EXPECT_EQ(owner->apply(access, respond), accepted);
    EXPECT_EQ(owner->episode_count(), 6);
    EXPECT_EQ(owner->acquisition_count(), 10);
    const auto episode = owner->episode({timeline, 6});
    ASSERT_TRUE(episode);
    EXPECT_EQ(episode->fact.kind, Story::EpisodeKind::Response);
    EXPECT_EQ(episode->support, (Story::EpisodeId{timeline, 5}));
    EXPECT_EQ(episode->audience[1], Story::PersonId{});
}

TEST(Storyactions, settlement_preparation_overflow_preserves_every_record) {
    auto owner = family();
    std::array<Story::CommitmentId, 17> commitments{};
    for (std::size_t index = 0; index < commitments.size(); ++index) {
        const auto offered = owner->apply(owner->actor(sera), request(index + 1, index + 1,
            Story::OfferPromise{liora, box, {100}, 1}));
        ASSERT_TRUE(offered);
        commitments[index] = offered.promise;
    }
    for (std::size_t index = 0; index < commitments.size(); ++index) {
        ASSERT_TRUE(owner->apply(owner->actor(liora), request(index + 1, index + 18,
            Story::DecidePromise{commitments[index], 1, true})));
    }
    ASSERT_TRUE(owner->apply(owner->actor(liora), request(18, 35, Story::ConsentTransfer{box, sera, 1})));
    const auto transfer = request(18, 36, Story::Transfer{box, liora, 2});
    const auto refused = owner->apply(owner->actor(sera), transfer);
    EXPECT_EQ(refused.error, Story::Error::WorkLimit);
    EXPECT_FALSE(refused.promise);
    EXPECT_EQ(owner->apply(owner->actor(sera), transfer), refused);
    EXPECT_EQ(owner->object(box)->holder, sera);
    EXPECT_EQ(owner->object(box)->version, 2);
    EXPECT_EQ(owner->object(box)->last_transfer, Story::EpisodeId{});
    for (const auto commitment : commitments) {
        EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Pending);
        EXPECT_EQ(owner->commitment(commitment)->version, 2);
        EXPECT_EQ(owner->commitment(commitment)->settlement, Story::EpisodeId{});
    }
    EXPECT_EQ(owner->episode_count(), 35);
    EXPECT_EQ(owner->acquisition_count(), 70);
}

TEST(Storyactions, competing_consent_stales_only_the_selected_object) {
    auto owner = family();
    const auto commitment = promise(*owner);
    ASSERT_TRUE(owner->apply(owner->actor(liora), request(2, 3, Story::ConsentTransfer{box, sera, 1})));
    ASSERT_TRUE(owner->apply(owner->actor(oren), request(1, 4, Story::ConsentTransfer{box, sera, 2})));
    const auto displaced = request(2, 5, Story::Transfer{box, liora, 2});
    EXPECT_EQ(owner->apply(owner->actor(sera), displaced).error, Story::Error::Stale);
    ASSERT_TRUE(owner->apply(owner->actor(sera), request(3, 6, Story::Transfer{box, oren, 3})));
    EXPECT_EQ(owner->object(box)->holder, oren);
    EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Pending);
}
