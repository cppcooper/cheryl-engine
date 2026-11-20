#include "fixture.h"
#include <iostream>

int main() {
    using namespace StoryFixture;
    auto owner = family();
    const auto trace = baseline(*owner);
    const auto promise = owner->commitment(trace.offered.promise);
    if (!trace.offered || !trace.accepted || !trace.consented || !trace.transferred || !trace.told ||
        trace.transferred != trace.duplicate || trace.refusal.error != Story::Error::NotHolder ||
        owner->object(box)->holder != liora || !promise || promise->state != Story::CommitmentState::Fulfilled ||
        trace.episodes != 5 || trace.acquisitions != 9 || trace.oren_count != 1 ||
        trace.oren_accounts[0].evidence != Story::Evidence::Attributed ||
        trace.oren_accounts[0].proposition.kind != Story::EpisodeKind::Transferred ||
        trace.oren_accounts[0].attributed_to != liora) {
        return 1;
    }
    auto substituted = family();
    const auto pending = StoryFixture::promise(*substituted);
    if (!substituted->apply(substituted->actor(liora), request(2, 3, Story::ConsentTransfer{substitute, sera, 1})) ||
        !substituted->apply(substituted->actor(sera), request(2, 4, Story::Transfer{substitute, liora, 2})) ||
        substituted->commitment(pending)->state != Story::CommitmentState::Pending ||
        substituted->object(box)->holder != sera) {
        return 2;
    }
    auto private_world = family();
    auto reference_world = family();
    if (StoryFixture::promise(*private_world) != StoryFixture::promise(*reference_world) ||
        !private_world->apply(private_world->actor(oren), request(1, 3, Story::ConsentTransfer{substitute, sera, 1})) ||
        !private_world->apply(private_world->actor(sera), request(2, 4, Story::Transfer{substitute, oren, 2}))) {
        return 3;
    }
    std::array<Story::Knowledge, 8> private_view{};
    std::array<Story::Knowledge, 8> reference_view{};
    const auto private_count = private_world->knowledge(private_world->actor(liora), private_view);
    const auto reference_count = reference_world->knowledge(reference_world->actor(liora), reference_view);
    const auto response = request(2, 5, Story::Respond{sera, 3, -1});
    const auto private_response = private_world->apply(private_world->actor(liora), response);
    const auto reference_response = reference_world->apply(reference_world->actor(liora), response);
    if (!private_count || !reference_count || *private_count != 2 || *reference_count != 2 ||
        private_view != reference_view || private_response != reference_response ||
        private_response.error != Story::Error::UnknownAccount) {
        return 4;
    }
    std::cout << "Sera offers exact keepsake 71 to Liora; Liora independently accepts and consents.\n"
                 "One transfer fulfills the promise. An identical retry repeats no effect.\n"
                 "Oren cannot transfer Liora's box; Oren learns only Liora's attributed account.\n"
                 "Substitution leaves the promise pending; hidden transfers do not change Liora's account or response.\n"
                 "episodes=5 acquisitions=9 oren_accounts=1 native trace from real Family calls\n";
    return 0;
}
