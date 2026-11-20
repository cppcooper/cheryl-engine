#pragma once

#include "../model/storage.h"

namespace CE::Story::Detail {
    template<class Record> struct Replacement {
        Record* target{};
        Record value{};
    };
    struct Prepared {
        Storage& store;
        PersonId actor;
        const Request& request;
        Result result;
        Episode episode;
        std::array<Replacement<CommitmentRecord>, 16> promises{};
        std::size_t promise_count{};
        std::optional<CommitmentRecord> added_promise;
        std::optional<Replacement<ObjectRecord>> object;
        std::optional<Replacement<ContactLink>> contact;
        std::array<Account, 2> accounts{};
        std::size_t account_count{};
        std::array<Replacement<PersonRecord>, 2> people{};
        std::size_t people_count{};
        std::array<Replacement<DueWork>, 16> due{};
        std::size_t due_count{};
        std::optional<DueWork> added_due;
        std::size_t work{};

        Prepared(Storage& owner, PersonId who, const Request& body)
            : store(owner), actor(who), request(body), result{Error::None, body.attempt},
              episode{{owner.timeline, owner.episodes.rows.size() + 1}, body.order, {}, {}, {who, {}}} {}
        bool step() noexcept {
            if (work == store.limits.work_per_action) {
                return false;
            }
            ++work;
            return true;
        }
        CommitmentRecord* promise(CommitmentId id, std::uint64_t version, Error& error) noexcept {
            if (!store.scoped(id)) {
                error = Error::ForeignReference;
                return nullptr;
            }
            auto* record = store.promises.find(id);
            count(store.counters.records_read);
            if (!record || (record->debtor != actor && record->creditor != actor)) {
                error = Error::Authority;
            } else if (record->version != version) {
                error = Error::Stale;
            } else if (record->version == std::numeric_limits<std::uint64_t>::max()) {
                error = Error::Capacity;
            } else {
                result.promise = id;
                result.state = record->state;
                return record;
            }
            return nullptr;
        }
        Error replace(CommitmentRecord* target, CommitmentRecord value) noexcept {
            if (promise_count == promises.size()) {
                return Error::WorkLimit;
            }
            ++value.version;
            promises[promise_count++] = {target, value};
            return Error::None;
        }
        Error settle_due(CommitmentId id) noexcept {
            const auto index = static_cast<std::size_t>(id.value - 1);
            if (index < store.due.size() && store.due[index].promise == id) {
                auto value = store.due[index];
                value.active = false;
                if (due_count == due.size()) {
                    return Error::WorkLimit;
                }
                due[due_count++] = {&store.due[index], value};
            }
            return Error::None;
        }
        Error acquire(PersonId recipient, Proposition fact, Evidence evidence, PersonId attribution, EpisodeId source) noexcept {
            if (account_count == accounts.size() || !store.accounts.room(account_count + 1)) {
                return Error::Capacity;
            }
            auto* original = store.people.find(recipient);
            if (!original || !store.alive(recipient, request.order.time)) {
                return Error::Life;
            }
            auto found = std::find_if(people.begin(), people.begin() + people_count,
                [&](const auto& edit) { return edit.target == original; });
            if (found == people.begin() + people_count) {
                if (people_count == people.size()) {
                    return Error::Capacity;
                }
                people[people_count++] = {original, *original};
            }
            auto& person = found->value;
            if (person.account_count == std::numeric_limits<std::uint64_t>::max()) {
                return Error::Capacity;
            }
            const auto local = ++person.account_count;
            accounts[account_count] = {{recipient, local}, {local, fact, attribution, evidence, request.order.time}, source, person.accounts};
            person.accounts = store.accounts.rows.size() + account_count++;
            return Error::None;
        }
        void publish() noexcept {
            for (std::size_t index = 0; index < promise_count; ++index) {
                *promises[index].target = promises[index].value;
            }
            if (added_promise) {
                store.promises.append(*added_promise);
            }
            if (object) {
                *object->target = object->value;
            }
            if (contact) {
                *contact->target = contact->value;
            }
            for (std::size_t index = 0; index < people_count; ++index) {
                *people[index].target = people[index].value;
            }
            for (std::size_t index = 0; index < due_count; ++index) {
                *due[index].target = due[index].value;
            }
            if (added_due) {
                store.due.push_back(*added_due);
            }
            store.episodes.append(episode);
            for (std::size_t index = 0; index < account_count; ++index) {
                store.accounts.append(accounts[index]);
            }
            store.receipts.append({{actor, request.attempt}, request, result});
            store.last = request.order;
            count(store.counters.records_touched, promise_count + people_count + account_count + due_count +
                (object ? 1 : 0) + (contact ? 1 : 0) + (added_promise ? 1 : 0) + (added_due ? 1 : 0) + 2);
        }
    };
    Error prepare(Prepared&, const OfferPromise&) noexcept;
    Error prepare(Prepared&, const DecidePromise&) noexcept;
    Error prepare(Prepared&, const RequestRelease&) noexcept;
    Error prepare(Prepared&, const DecideRelease&) noexcept;
    Error prepare(Prepared&, const CheckDeadline&) noexcept;
    Error prepare(Prepared&, const ConsentTransfer&) noexcept;
    Error prepare(Prepared&, const Transfer&) noexcept;
    Error prepare(Prepared&, const Perceive&) noexcept;
    Error prepare(Prepared&, const Tell&) noexcept;
    Error prepare(Prepared&, const Respond&) noexcept;
}
