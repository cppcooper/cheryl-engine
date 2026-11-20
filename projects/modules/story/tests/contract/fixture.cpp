#include "fixture.h"
#include <stdexcept>

namespace StoryFixture {
    Inputs::Inputs() {
        const std::array ids{sera, liora, oren};
        const std::array names{"Sera", "Liora", "Oren"};
        for (std::size_t index = 0; index < people.size(); ++index) {
            auto& person = people[index];
            person.id = ids[index];
            person.origin = {timeline, ids[index].value};
            person.address = {1, ids[index].value};
            person.name = Story::Text::make(names[index]).value();
            person.born = {index == 0 ? -60 : -30};
            person.lineage = index == 0 ? Story::Lineage::Founder : Story::Lineage::Descendant;
            if (index != 0) {
                person.parents[0] = sera;
            }
        }
        objects[0] = {box, sera};
        objects[1] = {substitute, sera};
        contacts = {{{sera, liora}, {liora, oren}}};
    }
    Story::Seed Inputs::seed() const { return {timeline, {0}, people, objects, contacts}; }
    std::unique_ptr<Story::Family> family(Story::Limits limits) {
        Inputs inputs;
        auto created = Story::Family::create(inputs.seed(), limits);
        if (!created) {
            throw std::runtime_error("valid Story fixture seed refused");
        }
        return std::move(*created);
    }
    Story::Request request(std::uint64_t attempt, std::int64_t time, Story::Action action) {
        return {attempt, {{time}, 0, attempt}, std::move(action)};
    }
    Story::CommitmentId promise(Story::Family& owner) {
        const auto offer = owner.apply(owner.actor(sera), request(1, 1, Story::OfferPromise{liora, box, {10}, 1}));
        if (!offer) {
            throw std::runtime_error("valid Story offer refused");
        }
        const auto accept = owner.apply(owner.actor(liora), request(1, 2, Story::DecidePromise{offer.promise, 1, true}));
        if (!accept) {
            throw std::runtime_error("valid Story acceptance refused");
        }
        return offer.promise;
    }
    Trace baseline(Story::Family& owner) {
        Trace trace;
        const auto sera_access = owner.actor(sera);
        const auto liora_access = owner.actor(liora);
        trace.offered = owner.apply(sera_access, request(1, 1, Story::OfferPromise{liora, box, {10}, 1}));
        trace.accepted = owner.apply(liora_access, request(1, 2, Story::DecidePromise{trace.offered.promise, 1, true}));
        trace.consented = owner.apply(liora_access, request(2, 3, Story::ConsentTransfer{box, sera, 1}));
        const auto transfer = request(2, 4, Story::Transfer{box, liora, 2});
        trace.transferred = owner.apply(sera_access, transfer);
        trace.duplicate = owner.apply(sera_access, transfer);
        trace.refusal = owner.apply(owner.actor(oren), request(1, 5, Story::Transfer{box, oren, 3}));
        trace.told = owner.apply(liora_access, request(3, 6, Story::Tell{oren, 4}));
        trace.episodes = owner.episode_count();
        trace.acquisitions = owner.acquisition_count();
        const auto known = owner.knowledge(owner.actor(oren), trace.oren_accounts);
        trace.oren_count = known.value_or(0);
        return trace;
    }
}
