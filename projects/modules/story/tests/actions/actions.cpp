#include <contract/fixture.h>
#include <gtest/gtest.h>

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
