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
    std::cout << "Sera offers exact keepsake 71 to Liora; Liora independently accepts and consents.\n"
                 "One transfer fulfills the promise. An identical retry repeats no effect.\n"
                 "Oren cannot transfer Liora's box; Oren learns only Liora's attributed account.\n"
                 "episodes=5 acquisitions=9 oren_accounts=1 native trace from real Family calls\n";
    return 0;
}
