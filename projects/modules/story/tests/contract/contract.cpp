#include "fixture.h"
#include <cheryl/story/generation.h>
#include <cheryl/story/timeline.h>
#include <cheryl/story/narrative.h>
#include <gtest/gtest.h>

using namespace StoryFixture;

TEST(Storycontract, real_promise_transfer_and_account) {
    auto owner = family();
    const auto trace = baseline(*owner);
    ASSERT_TRUE(trace.offered);
    ASSERT_TRUE(trace.accepted);
    ASSERT_TRUE(trace.consented);
    ASSERT_TRUE(trace.transferred);
    EXPECT_EQ(trace.duplicate, trace.transferred);
    EXPECT_EQ(trace.refusal.error, Story::Error::NotHolder);
    ASSERT_TRUE(trace.told);
    EXPECT_EQ(owner->object(box)->holder, liora);
    EXPECT_EQ(owner->commitment(trace.offered.promise)->state, Story::CommitmentState::Fulfilled);
    EXPECT_EQ(owner->commitment(trace.offered.promise)->settlement, (Story::EpisodeId{timeline, 4}));
    EXPECT_EQ(trace.episodes, 5);
    EXPECT_EQ(trace.acquisitions, 9);
    ASSERT_EQ(trace.oren_count, 1);
    EXPECT_EQ(trace.oren_accounts[0].local_sequence, 1);
    EXPECT_EQ(trace.oren_accounts[0].evidence, Story::Evidence::Attributed);
    EXPECT_EQ(trace.oren_accounts[0].attributed_to, liora);
    EXPECT_EQ(trace.oren_accounts[0].proposition.kind, Story::EpisodeKind::Transferred);
}

TEST(Storycontract, alternate_recipient_does_not_fulfill_liora_promise) {
    auto owner = family();
    const auto commitment = promise(*owner);
    ASSERT_TRUE(owner->apply(owner->actor(oren), request(1, 3, Story::ConsentTransfer{box, sera, 1})));
    ASSERT_TRUE(owner->apply(owner->actor(sera), request(2, 4, Story::Transfer{box, oren, 2})));
    EXPECT_EQ(owner->object(box)->holder, oren);
    EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Pending);
    EXPECT_EQ(owner->commitment(commitment)->settlement, Story::EpisodeId{});
    std::array<Story::Knowledge, 8> visible;
    const auto known = owner->knowledge(owner->actor(liora), visible);
    ASSERT_TRUE(known);
    EXPECT_EQ(*known, 2);
    EXPECT_EQ(visible[1].proposition.kind, Story::EpisodeKind::PromiseAccepted);
    const auto response = request(2, 5, Story::Respond{sera, 3, -2});
    EXPECT_EQ(owner->apply(owner->actor(liora), response).error, Story::Error::UnknownAccount);
}

TEST(Storycontract, private_noninterference) {
    auto first = family();
    auto second = family();
    ASSERT_EQ(promise(*first), promise(*second));
    ASSERT_TRUE(first->apply(first->actor(oren), request(1, 3, Story::ConsentTransfer{substitute, sera, 1})));
    ASSERT_TRUE(first->apply(first->actor(sera), request(2, 4, Story::Transfer{substitute, oren, 2})));
    std::array<Story::Knowledge, 8> left;
    std::array<Story::Knowledge, 8> right;
    const auto left_count = first->knowledge(first->actor(liora), left);
    const auto right_count = second->knowledge(second->actor(liora), right);
    ASSERT_EQ(left_count, right_count);
    EXPECT_EQ(left, right);
    const auto hidden_response = request(2, 5, Story::Respond{sera, 3, -1});
    EXPECT_EQ(first->apply(first->actor(liora), hidden_response), second->apply(second->actor(liora), hidden_response));
}

TEST(Storycontract, owned_receipt_outlives_inputs_and_owner) {
    Story::Result retained;
    {
        auto owner = family();
        retained = owner->apply(owner->actor(sera), request(1, 1, Story::OfferPromise{liora, box, {10}, 1}));
    }
    EXPECT_EQ(retained.promise, (Story::CommitmentId{timeline, 1}));
    EXPECT_EQ(retained.state, Story::CommitmentState::Offered);
    EXPECT_EQ(Story::GenerationResult{}.status, Story::GenerationStatus::Unsupported);
}
