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
    Story::NewPerson newcomer(std::uint64_t identity) {
        return {{timeline, identity}, {7, identity}, Story::Text::make("Arrival").value()};
    }
    Story::LifeRequest life_request(std::uint64_t attempt, std::int64_t time, Story::LifeAction action) {
        return {attempt, {{time}, 0, attempt}, 1, std::move(action)};
    }
    Story::Result agreement(Story::Family& owner, Story::PersonId first, Story::PersonId second,
        std::int64_t time, std::uint64_t first_attempt, std::uint64_t second_attempt) {
        const auto permit = owner.permit(owner.life(), {first, second, owner.person(first)->version,
            owner.person(second)->version, 1, {time}, 18, true, true});
        if (!permit) {
            return {permit.error()};
        }
        const auto proposed = owner.apply(owner.actor(first), request(first_attempt, time, Story::ProposePartnership{*permit}));
        if (!proposed) {
            return proposed;
        }
        return owner.apply(owner.actor(second), request(second_attempt, time + 1,
            Story::DecidePartnership{proposed.partnership, 1, true}));
    }
    LifeTrace dynamic_life(Story::Family& owner) {
        LifeTrace trace;
        trace.introduced = owner.apply(owner.life(), life_request(1, 1, Story::Introduce{newcomer(1001), {-25}, liora, 1}));
        if (!trace.introduced) { return trace; }
        const auto incoming = trace.introduced.person;
        trace.accepted = agreement(owner, liora, incoming, 2);
        if (!trace.accepted) { return trace; }
        trace.born = owner.apply(owner.life(), life_request(2, 4,
            Story::Birth{newcomer(1002), trace.accepted.partnership, 2, {1, 1}, 18, true}));
        if (!trace.born) { return trace; }
        const auto child = trace.born.person;
        const auto access = owner.actor(child);
        std::array<Story::Knowledge, 8> accounts{};
        trace.newborn_accounts = owner.knowledge(access, accounts).value_or(99);
        if (!owner.apply(access, request(1, 5, Story::ConsentTransfer{box, sera, 1}))) { return trace; }
        trace.transferred = owner.apply(owner.actor(sera), request(1, 6, Story::Transfer{box, child, 2}));
        trace.told = owner.apply(owner.actor(liora), request(2, 7, Story::Tell{child, 3}));
        trace.responded = owner.apply(access, request(2, 8, Story::Respond{liora, 3, 1}));
        trace.left = owner.apply(access, request(3, 9, Story::SetMembership{1, false}));
        trace.rejoined = owner.apply(access, request(4, 10, Story::SetMembership{2, true}));
        trace.died = owner.apply(owner.life(), life_request(3, 11, Story::Death{incoming, 1}));
        trace.complete = trace.transferred && trace.told && trace.responded && trace.left && trace.rejoined && trace.died;
        return trace;
    }
}
