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

TEST(Storylife, dynamic_growth_participates_in_existing_actions_without_inherited_accounts) {
    auto owner = family();
    const auto trace = dynamic_life(*owner);
    ASSERT_TRUE(trace.complete);
    EXPECT_EQ(trace.introduced.person, (Story::PersonId{timeline, 901}));
    EXPECT_EQ(trace.born.person, (Story::PersonId{timeline, 902}));
    EXPECT_FALSE(owner->person(trace.introduced.person)->depth);
    const auto child = owner->person(trace.born.person);
    ASSERT_TRUE(child);
    EXPECT_EQ(child->parents, (std::array{liora, trace.introduced.person}));
    EXPECT_EQ(child->depth, 2);
    EXPECT_EQ(child->birth_support, trace.accepted.partnership);
    EXPECT_EQ(child->source, trace.born.episode);
    EXPECT_EQ(owner->episode(trace.born.episode)->support, trace.accepted.episode);
    EXPECT_EQ(trace.newborn_accounts, 0);
    EXPECT_EQ(owner->object(box)->holder, child->id);
    EXPECT_TRUE(child->member);
    EXPECT_FALSE(child->died);
    EXPECT_EQ(owner->partnership(trace.accepted.partnership)->state, Story::PartnershipState::Ended);
    const auto current = owner->membership(child->membership);
    ASSERT_TRUE(current);
    EXPECT_EQ(current->since, (Story::StoryTime{10}));
    EXPECT_EQ(current->source, trace.rejoined.episode);
    const auto previous = owner->membership(current->previous);
    ASSERT_TRUE(previous);
    EXPECT_EQ(previous->since, (Story::StoryTime{4}));
    EXPECT_EQ(previous->until, (Story::StoryTime{9}));
    EXPECT_EQ(previous->ending, trace.left.episode);
    EXPECT_EQ(owner->person(trace.introduced.person)->died, (Story::StoryTime{11}));
    EXPECT_EQ(owner->counters().validation_passes, 1);
    EXPECT_EQ(owner->counters().whole_state_copies, 0);
}

TEST(Storylife, host_authority_retry_namespaces_and_retained_owned_result) {
    Story::Result retained;
    Story::LifeCapability stale;
    {
        auto owner = family();
        auto other = family();
        const auto introduction = life_request(1, 1, Story::Introduce{newcomer(1001), {-25}, liora, 1});
        EXPECT_EQ(owner->apply(Story::LifeCapability{}, introduction).error, Story::Error::Authority);
        EXPECT_EQ(owner->apply(other->life(), introduction).error, Story::Error::Authority);
        retained = owner->apply(owner->life(), introduction);
        ASSERT_TRUE(retained);
        stale = owner->life();
        const auto actor_result = owner->apply(owner->actor(liora), request(1, 2, Story::ConsentTransfer{box, sera, 1}));
        ASSERT_TRUE(actor_result);
        EXPECT_EQ(owner->apply(owner->life(), introduction), retained);
        auto changed = introduction;
        changed.action = Story::Death{sera, 1};
        EXPECT_EQ(owner->apply(owner->life(), changed).error, Story::Error::Conflict);
        ASSERT_EQ(owner->retire_through(owner->life(), 1), Story::Error::None);
        EXPECT_EQ(owner->apply(owner->life(), introduction).error, Story::Error::Expired);
        EXPECT_EQ(owner->apply(owner->actor(liora), request(1, 2, Story::ConsentTransfer{box, sera, 1})), actor_result);
        EXPECT_EQ(owner->episode_count(), 2);
    }
    auto replacement = family();
    EXPECT_EQ(replacement->apply(stale, life_request(2, 3, Story::Death{sera, 1})).error, Story::Error::Authority);
    EXPECT_EQ(retained.person, (Story::PersonId{timeline, 901}));
    EXPECT_EQ(retained.episode, (Story::EpisodeId{timeline, 1}));
}

TEST(Storylife, introduction_payload_identity_time_and_version_gates_are_atomic) {
    auto owner = family();
    auto incoming = newcomer(1001);
    incoming.origin = owner->person(liora)->origin;
    EXPECT_EQ(owner->apply(owner->life(), life_request(1, 1, Story::Introduce{incoming, {-25}})).error,
        Story::Error::DuplicateOrigin);
    incoming = newcomer(1001);
    incoming.address = owner->person(liora)->address;
    EXPECT_EQ(owner->apply(owner->life(), life_request(2, 1, Story::Introduce{incoming, {-25}})).error,
        Story::Error::DuplicateCreation);
    incoming = newcomer(1001);
    incoming.name.size = 97;
    EXPECT_EQ(owner->apply(owner->life(), life_request(3, 1, Story::Introduce{incoming, {-25}})).error, Story::Error::Terms);
    EXPECT_EQ(owner->apply(owner->life(), life_request(4, 1, Story::Introduce{newcomer(1001), {2}})).error, Story::Error::Life);
    EXPECT_EQ(owner->apply(owner->life(), life_request(5, 1, Story::Introduce{newcomer(1001), {-25}, liora, 2})).error,
        Story::Error::Stale);
    EXPECT_EQ(owner->episode_count(), 0);
    EXPECT_FALSE(owner->person({timeline, 901}));
    const auto result = owner->apply(owner->life(), life_request(6, 1, Story::Introduce{newcomer(1001), {-25}}));
    ASSERT_TRUE(result);
    EXPECT_EQ(result.person, (Story::PersonId{timeline, 901}));
}

TEST(Storylife, explicit_policy_and_independent_partner_decision) {
    auto owner = family();
    auto other = family();
    Story::PartnershipEligibility eligibility{liora, oren, 1, 1, 1, {1}, 18, true, true};
    EXPECT_EQ(owner->permit(other->life(), eligibility).error(), Story::Error::Authority);
    eligibility.kinship_allowed = false;
    EXPECT_EQ(owner->permit(owner->life(), eligibility).error(), Story::Error::Policy);
    eligibility.kinship_allowed = true;
    eligibility.minimum_age = 40;
    EXPECT_EQ(owner->permit(owner->life(), eligibility).error(), Story::Error::Life);
    eligibility.minimum_age = 18;
    const auto permit = owner->permit(owner->life(), eligibility);
    ASSERT_TRUE(permit);
    EXPECT_EQ(owner->apply(owner->actor(liora), request(1, 1, Story::ProposePartnership{})).error, Story::Error::Authority);
    const auto foreign = other->permit(other->life(), eligibility);
    ASSERT_TRUE(foreign);
    EXPECT_EQ(owner->apply(owner->actor(liora), request(2, 1, Story::ProposePartnership{*foreign})).error, Story::Error::Authority);
    const auto proposal = owner->apply(owner->actor(liora), request(3, 1, Story::ProposePartnership{*permit}));
    ASSERT_TRUE(proposal);
    EXPECT_EQ(owner->partnership(proposal.partnership)->state, Story::PartnershipState::Proposed);
    EXPECT_EQ(owner->apply(owner->life(), life_request(1, 2,
        Story::Birth{newcomer(1001), proposal.partnership, 1, {1, 1}, 18, true})).error, Story::Error::Consent);
    EXPECT_EQ(owner->apply(owner->actor(liora), request(4, 2,
        Story::DecidePartnership{proposal.partnership, 1, true})).error, Story::Error::Authority);
    ASSERT_TRUE(owner->apply(owner->actor(oren), request(1, 2, Story::DecidePartnership{proposal.partnership, 1, false})));
    const auto declined = owner->partnership(proposal.partnership);
    ASSERT_TRUE(declined);
    EXPECT_EQ(declined->state, Story::PartnershipState::Declined);
    EXPECT_FALSE(declined->accepted);
    EXPECT_TRUE(declined->ended);
    EXPECT_FALSE(owner->person({timeline, 901}));
}

TEST(Storylife, partnership_withdrawal_retains_consent_history_and_blocks_birth) {
    auto owner = family();
    const auto accepted = agreement(*owner, liora, oren, 1);
    ASSERT_TRUE(accepted);
    const auto ended = owner->apply(owner->actor(liora), request(2, 3, Story::EndPartnership{accepted.partnership, 2}));
    ASSERT_TRUE(ended);
    EXPECT_EQ(owner->partnership(accepted.partnership)->accepted, accepted.episode);
    EXPECT_EQ(owner->partnership(accepted.partnership)->ended, ended.episode);
    EXPECT_EQ(owner->apply(owner->life(), life_request(1, 4,
        Story::Birth{newcomer(1001), accepted.partnership, 3, {1, 1}, 18, true})).error, Story::Error::Consent);
    EXPECT_EQ(owner->apply(owner->actor(oren), request(2, 4,
        Story::EndPartnership{accepted.partnership, 3})).error, Story::Error::Terminal);
    EXPECT_EQ(owner->episode_count(), 3);
}

TEST(Storylife, birth_rechecks_policy_parent_stamps_and_maximum_parent_depth) {
    auto owner = family();
    const auto accepted = agreement(*owner, sera, liora, 1);
    ASSERT_TRUE(accepted);
    auto birth = Story::Birth{newcomer(1001), accepted.partnership, 2, {1, 1}, 18, false};
    EXPECT_EQ(owner->apply(owner->life(), life_request(1, 3, birth)).error, Story::Error::Policy);
    birth.allowed = true;
    birth.parent_versions[1] = 2;
    EXPECT_EQ(owner->apply(owner->life(), life_request(2, 3, birth)).error, Story::Error::Stale);
    birth.parent_versions[1] = 1;
    birth.minimum_parent_age = 100;
    EXPECT_EQ(owner->apply(owner->life(), life_request(3, 3, birth)).error, Story::Error::Life);
    birth.minimum_parent_age = 18;
    auto wrong_policy = life_request(4, 3, birth);
    wrong_policy.policy_version = 2;
    EXPECT_EQ(owner->apply(owner->life(), wrong_policy).error, Story::Error::Policy);
    const auto born = owner->apply(owner->life(), life_request(5, 3, birth));
    ASSERT_TRUE(born);
    EXPECT_EQ(owner->person(born.person)->depth, 2);
    EXPECT_EQ(owner->person(born.person)->parents, (std::array{sera, liora}));
    EXPECT_EQ(owner->episode_count(), 3);
}

TEST(Storylife, departure_invalidates_old_eligibility_without_dying_or_erasing_accounts) {
    auto owner = family();
    const auto permit = owner->permit(owner->life(), {liora, oren, 1, 1, 1, {1}, 18, true, true});
    ASSERT_TRUE(permit);
    const auto proposed = owner->apply(owner->actor(liora), request(1, 1, Story::ProposePartnership{*permit}));
    ASSERT_TRUE(proposed);
    ASSERT_TRUE(owner->apply(owner->actor(oren), request(1, 2, Story::SetMembership{1, false})));
    EXPECT_FALSE(owner->person(oren)->died);
    EXPECT_FALSE(owner->person(oren)->member);
    EXPECT_EQ(owner->partnership(proposed.partnership)->state, Story::PartnershipState::Cancelled);
    ASSERT_TRUE(owner->apply(owner->actor(oren), request(2, 3, Story::SetMembership{2, true})));
    EXPECT_EQ(owner->permit(owner->life(), {liora, oren, 1, 1, 1, {4}, 18, true, true}).error(), Story::Error::Stale);
    std::array<Story::Knowledge, 8> accounts{};
    ASSERT_EQ(owner->knowledge(owner->actor(oren), accounts).value(), 2);
    EXPECT_EQ(accounts[0].proposition.kind, Story::EpisodeKind::PartnershipProposed);
    EXPECT_EQ(accounts[1].proposition.kind, Story::EpisodeKind::Rejoined);
}

TEST(Storylife, death_fails_pending_and_offered_work_but_retains_terminal_settlement_and_retry) {
    auto owner = family();
    const auto pending = promise(*owner);
    const auto offered = owner->apply(owner->actor(sera), request(2, 3, Story::OfferPromise{oren, substitute, {20}, 1}));
    ASSERT_TRUE(offered);
    const auto death = life_request(1, 4, Story::Death{sera, 1});
    const auto died = owner->apply(owner->life(), death);
    ASSERT_TRUE(died);
    EXPECT_EQ(owner->apply(owner->life(), death), died);
    EXPECT_EQ(owner->commitment(pending)->state, Story::CommitmentState::Failed);
    EXPECT_EQ(owner->commitment(offered.promise)->state, Story::CommitmentState::Failed);
    EXPECT_EQ(owner->commitment(pending)->settlement, died.episode);
    EXPECT_EQ(owner->apply(owner->actor(liora), request(2, 21, Story::CheckDeadline{pending, 3})).error, Story::Error::Terms);
    EXPECT_EQ(owner->object(box)->holder, sera);
    auto terminal = family();
    const auto trace = baseline(*terminal);
    ASSERT_TRUE(trace.transferred);
    const auto settlement = terminal->commitment(trace.offered.promise)->settlement;
    ASSERT_TRUE(terminal->apply(terminal->life(), life_request(1, 7, Story::Death{sera, 1})));
    EXPECT_EQ(terminal->commitment(trace.offered.promise)->state, Story::CommitmentState::Fulfilled);
    EXPECT_EQ(terminal->commitment(trace.offered.promise)->settlement, settlement);
    EXPECT_EQ(terminal->object(box)->holder, liora);
}

TEST(Storylife, high_degree_loss_refuses_without_partial_death_or_settlement) {
    auto owner = family();
    std::array<Story::CommitmentId, 17> commitments{};
    for (std::size_t index = 0; index < commitments.size(); ++index) {
        const auto offered = owner->apply(owner->actor(sera), request(index + 1, index + 1,
            Story::OfferPromise{liora, box, {100}, 1}));
        ASSERT_TRUE(offered);
        commitments[index] = offered.promise;
    }
    const auto before = owner->acquisition_count();
    const auto death = life_request(1, 18, Story::Death{sera, 1});
    const auto refused = owner->apply(owner->life(), death);
    EXPECT_EQ(refused.error, Story::Error::WorkLimit);
    EXPECT_EQ(owner->apply(owner->life(), death), refused);
    EXPECT_FALSE(refused.person);
    EXPECT_FALSE(refused.episode);
    EXPECT_FALSE(owner->person(sera)->died);
    EXPECT_TRUE(owner->person(sera)->member);
    EXPECT_EQ(owner->person(sera)->version, 1);
    EXPECT_EQ(owner->episode_count(), 17);
    EXPECT_EQ(owner->acquisition_count(), before);
    for (const auto commitment : commitments) {
        EXPECT_EQ(owner->commitment(commitment)->state, Story::CommitmentState::Offered);
        EXPECT_EQ(owner->commitment(commitment)->version, 1);
        EXPECT_FALSE(owner->commitment(commitment)->settlement);
    }
    EXPECT_GE(owner->counters().links_traversed, 17);
}

TEST(Storylife, introduction_limits_refuse_without_consuming_identity_or_episode) {
    for (const auto limit : {0, 1, 2, 3}) {
        Story::Limits limits;
        if (limit == 0) { limits.people = 3; }
        if (limit == 1) { limits.memberships = 3; }
        if (limit == 2) { limits.links = 6; }
        if (limit == 3) { limits.acquisitions = 0; }
        auto owner = family(limits);
        const auto introduction = life_request(1, 1, Story::Introduce{newcomer(1001), {-25}, liora, 1});
        const auto refused = owner->apply(owner->life(), introduction);
        EXPECT_EQ(refused.error, Story::Error::Capacity);
        EXPECT_EQ(owner->apply(owner->life(), introduction), refused);
        EXPECT_EQ(owner->episode_count(), 0);
        EXPECT_EQ(owner->acquisition_count(), 0);
        EXPECT_FALSE(owner->person({timeline, 901}));
    }
}

TEST(Storylife, birth_link_and_acquisition_failure_preserve_accepted_partnership) {
    for (const auto links_limited : {false, true}) {
        Story::Limits limits;
        if (links_limited) { limits.links = 13; } else { limits.acquisitions = 5; }
        auto owner = family(limits);
        const auto accepted = agreement(*owner, liora, oren, 1);
        ASSERT_TRUE(accepted);
        const auto birth = life_request(1, 3, Story::Birth{newcomer(1001), accepted.partnership, 2, {1, 1}, 18, true});
        EXPECT_EQ(owner->apply(owner->life(), birth).error, Story::Error::Capacity);
        EXPECT_EQ(owner->episode_count(), 2);
        EXPECT_EQ(owner->acquisition_count(), 4);
        EXPECT_FALSE(owner->person({timeline, 901}));
        EXPECT_EQ(owner->partnership(accepted.partnership)->state, Story::PartnershipState::Active);
    }
}

TEST(Storylife, untracked_parents_do_not_manufacture_a_root_relative_descendant) {
    auto owner = family();
    const auto first = owner->apply(owner->life(), life_request(1, 1, Story::Introduce{newcomer(1001), {-30}}));
    const auto second = owner->apply(owner->life(), life_request(2, 2, Story::Introduce{newcomer(1002), {-30}}));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    const auto accepted = agreement(*owner, first.person, second.person, 3);
    ASSERT_TRUE(accepted);
    EXPECT_EQ(owner->apply(owner->life(), life_request(3, 5,
        Story::Birth{newcomer(1003), accepted.partnership, 2, {1, 1}, 18, true})).error, Story::Error::Depth);
    EXPECT_FALSE(owner->person({timeline, 903}));
    EXPECT_FALSE(owner->person(first.person)->depth);
    EXPECT_EQ(owner->episode_count(), 4);
}

TEST(Storylife, dependent_work_and_acquisition_limits_refuse_the_entire_loss) {
    Story::Limits limits;
    limits.work_per_action = 1;
    auto bounded = family(limits);
    const auto first = bounded->apply(bounded->actor(sera), request(1, 1, Story::OfferPromise{liora, box, {20}, 1}));
    const auto second = bounded->apply(bounded->actor(sera), request(2, 2, Story::OfferPromise{oren, substitute, {20}, 1}));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(bounded->apply(bounded->life(), life_request(1, 3, Story::Death{sera, 1})).error, Story::Error::WorkLimit);
    EXPECT_FALSE(bounded->person(sera)->died);
    EXPECT_EQ(bounded->commitment(first.promise)->state, Story::CommitmentState::Offered);
    EXPECT_EQ(bounded->commitment(second.promise)->state, Story::CommitmentState::Offered);
    EXPECT_EQ(bounded->acquisition_count(), 4);
    limits = {};
    limits.acquisitions = 4;
    auto full = family(limits);
    const auto pending = promise(*full);
    EXPECT_EQ(full->apply(full->life(), life_request(1, 3, Story::Death{sera, 1})).error, Story::Error::Capacity);
    EXPECT_FALSE(full->person(sera)->died);
    EXPECT_EQ(full->commitment(pending)->state, Story::CommitmentState::Pending);
    EXPECT_EQ(full->episode_count(), 2);
}

TEST(Storylife, loss_preserves_released_and_breached_work_and_rejects_second_occurrence) {
    for (const auto release : {false, true}) {
        auto owner = family();
        const auto pending = promise(*owner);
        if (release) {
            ASSERT_TRUE(owner->apply(owner->actor(sera), request(2, 3, Story::RequestRelease{pending, 2})));
            ASSERT_TRUE(owner->apply(owner->actor(liora), request(2, 4, Story::DecideRelease{pending, 3, true})));
        } else {
            ASSERT_TRUE(owner->apply(owner->actor(liora), request(2, 11, Story::CheckDeadline{pending, 2})));
        }
        const auto before = owner->commitment(pending);
        ASSERT_TRUE(before);
        const auto death = owner->apply(owner->life(), life_request(1, 12, Story::Death{sera, 1}));
        ASSERT_TRUE(death);
        EXPECT_EQ(owner->commitment(pending)->state, release ? Story::CommitmentState::Released : Story::CommitmentState::Breached);
        EXPECT_EQ(owner->commitment(pending)->version, before->version);
        EXPECT_EQ(owner->commitment(pending)->settlement, before->settlement);
        EXPECT_EQ(owner->apply(owner->life(), life_request(2, 13, Story::Death{sera, 2})).error, Story::Error::Life);
        EXPECT_EQ(owner->person(sera)->died, (Story::StoryTime{12}));
    }
}

TEST(Storylife, retained_receipts_membership_and_partnership_capacities_are_explicit) {
    Story::Limits limits;
    limits.receipts = 1;
    auto retained = family(limits);
    const auto introduction = life_request(1, 1, Story::Introduce{newcomer(1001), {-25}});
    const auto original = retained->apply(retained->life(), introduction);
    ASSERT_TRUE(original);
    EXPECT_EQ(retained->apply(retained->life(), life_request(2, 2, Story::Death{sera, 1})).error, Story::Error::Capacity);
    EXPECT_EQ(retained->apply(retained->life(), introduction), original);
    EXPECT_FALSE(retained->person(sera)->died);
    limits = {};
    limits.memberships = 3;
    auto membership_full = family(limits);
    ASSERT_TRUE(membership_full->apply(membership_full->actor(oren), request(1, 1, Story::SetMembership{1, false})));
    EXPECT_EQ(membership_full->apply(membership_full->actor(oren), request(2, 2, Story::SetMembership{2, true})).error,
        Story::Error::Capacity);
    EXPECT_FALSE(membership_full->person(oren)->member);
    EXPECT_EQ(membership_full->person(oren)->version, 2);
    limits = {};
    limits.partnerships = 0;
    auto partnership_full = family(limits);
    EXPECT_EQ(agreement(*partnership_full, liora, oren, 1).error, Story::Error::Capacity);
    EXPECT_EQ(partnership_full->episode_count(), 0);
}

TEST(Storylife, actual_loss_invalidates_birth_but_does_not_replay_prior_birth) {
    auto owner = family();
    const auto accepted = agreement(*owner, liora, oren, 1);
    ASSERT_TRUE(accepted);
    const auto birth = life_request(1, 3, Story::Birth{newcomer(1001), accepted.partnership, 2, {1, 1}, 18, true});
    const auto born = owner->apply(owner->life(), birth);
    ASSERT_TRUE(born);
    ASSERT_TRUE(owner->apply(owner->life(), life_request(2, 4, Story::Death{oren, 1})));
    EXPECT_EQ(owner->apply(owner->life(), birth), born);
    EXPECT_EQ(owner->apply(owner->life(), life_request(3, 5,
        Story::Birth{newcomer(1002), accepted.partnership, 3, {1, 2}, 18, true})).error, Story::Error::Consent);
    EXPECT_FALSE(owner->person({timeline, 902}));
    EXPECT_EQ(owner->person(born.person)->parents[1], oren);
    EXPECT_EQ(owner->episode_count(), 4);
}
