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

TEST(Storycontract, unknown_account_refusal_is_private) {
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

TEST(Storycontract, generation_values_retain_explicit_seed_pins_and_local_frontiers) {
    EXPECT_FALSE(Story::GenerationSpec{}.seed);
    Story::Continuation retained;
    {
        Story::GenerationSpec specification;
        specification.seed = 0;
        specification.seed_namespace = 17;
        specification.lineage = {17, 1};
        specification.versions.content = 2;
        specification.versions.policy = 3;
        specification.versions.generator = 4;
        specification.versions.randomness = 5;
        specification.versions.selection_mapping = 6;
        specification.generation_count = 3;
        specification.end_time = {200};
        specification.choices = {8, 16, 32, 4};
        retained.specification = specification;
        retained.timeline = timeline;
        retained.frontier = {{7}, 1, 3};
        retained.accepted_sequence = 9;
        Story::DecisionCursor cursor;
        cursor.address = {Story::RandomStream::Decision, {17, 21}, 4, 2, Story::GenerationAddress{17, 22}};
        cursor.next_draw = 3;
        cursor.candidate_frontier = Story::GenerationAddress{17, 22};
        retained.decisions.push_back(cursor);
        Story::ScheduledIntent pending;
        pending.order = {{10}, 1, 4};
        pending.actor = liora;
        pending.action = Story::Tell{oren, 1};
        pending.address = {17, 30};
        pending.decision = cursor.address;
        pending.expected_version = 2;
        retained.pending.push_back(pending);
        retained.choice_work = {1, 3, 5, 2};
        specification.seed = 99;
        cursor.next_draw = 99;
        pending.decision.occurrence = 99;
    }
    ASSERT_TRUE(retained.specification.seed);
    EXPECT_EQ(*retained.specification.seed, 0);
    EXPECT_EQ(retained.specification.seed_namespace, 17);
    EXPECT_EQ(retained.specification.versions.generator, 4);
    EXPECT_EQ(retained.specification.versions.randomness, 5);
    EXPECT_EQ(retained.specification.versions.selection_mapping, 6);
    EXPECT_EQ(retained.specification.versions.policy, 3);
    EXPECT_EQ(retained.specification.versions.content, 2);
    EXPECT_EQ(retained.specification.generation_count, 3);
    EXPECT_EQ(retained.specification.end_time, (Story::StoryTime{200}));
    EXPECT_EQ(retained.specification.choices, (Story::ChoiceLimits{8, 16, 32, 4}));
    ASSERT_EQ(retained.decisions.size(), 1);
    ASSERT_EQ(retained.pending.size(), 1);
    EXPECT_EQ(retained.decisions[0].next_draw, 3);
    EXPECT_EQ(retained.pending[0].decision.occurrence, 2);
    EXPECT_EQ(retained.pending[0].decision, retained.decisions[0].address);
    EXPECT_EQ(retained.choice_work, (Story::ChoiceCounters{1, 3, 5, 2}));
    Story::TimelineCut cut{timeline, retained.accepted_sequence, retained.frontier, retained.specification};
    cut.source = {42};
    EXPECT_EQ(cut.specification, retained.specification);
    EXPECT_EQ(Story::GenerationResult{}.status, Story::GenerationStatus::Unsupported);
}

TEST(Storycontract, decision_address_values_separate_stable_choice_domains) {
    const Story::DecisionAddress address{Story::RandomStream::Decision, {17, 21}, 4, 2,
        Story::GenerationAddress{17, 22}};
    auto changed = address;
    changed.stream = Story::RandomStream::Life;
    EXPECT_NE(changed, address);
    changed.stream = Story::RandomStream::Wording;
    EXPECT_NE(changed, address);
    changed = address;
    changed.subject.creation = 23;
    EXPECT_NE(changed, address);
    changed = address;
    ++changed.kind;
    EXPECT_NE(changed, address);
    changed = address;
    ++changed.occurrence;
    EXPECT_NE(changed, address);
    changed = address;
    changed.candidate = Story::GenerationAddress{17, 24};
    EXPECT_NE(changed, address);
    changed.candidate.reset();
    EXPECT_NE(changed, address);
    Story::DecisionCursor first{address, 3, address.candidate};
    Story::DecisionCursor unrelated{changed, 900, std::nullopt};
    ++unrelated.next_draw;
    EXPECT_EQ(first.next_draw, 3);
    EXPECT_EQ(first.address, address);
}
