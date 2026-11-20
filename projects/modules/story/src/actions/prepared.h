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
        std::variant<Request, LifeRequest> body;
        EventOrder order;
        Result result;
        Episode episode;
        std::array<Replacement<CommitmentRecord>, 16> promises{};
        std::size_t promise_count{};
        std::optional<CommitmentRecord> added_promise;
        std::optional<Replacement<ObjectRecord>> object;
        std::optional<Replacement<ContactLink>> contact;
        std::array<Account, 34> accounts{};
        std::size_t account_count{};
        std::array<Replacement<PersonRecord>, 36> people{};
        std::size_t people_count{};
        std::array<Replacement<DueWork>, 16> due{};
        std::size_t due_count{};
        std::optional<DueWork> added_due;
        std::optional<PersonRecord> added_person;
        std::optional<Partnership> added_partnership;
        std::array<Replacement<Partnership>, 16> partnerships{};
        std::size_t partnership_count{};
        std::optional<Membership> added_membership;
        std::optional<Replacement<Membership>> membership;
        std::array<ContactLink, 4> added_contacts{};
        std::size_t contact_count{};
        std::array<RelationLink, 4> added_relations{};
        std::size_t relation_count{};
        std::size_t work{};

        Prepared(Storage& owner, PersonId who, const auto& request)
            : store(owner), actor(who), body(request), order(request.order), result{Error::None, request.attempt},
              episode{{owner.timeline, owner.episodes.rows.size() + 1}, request.order, {}, {}, {who, {}}} {}
        PersonRecord* edit(PersonId id) noexcept {
            if (added_person && added_person->id == id) {
                return &*added_person;
            }
            auto* original = store.people.find(id);
            if (!original) {
                return nullptr;
            }
            for (std::size_t index = 0; index < people_count; ++index) {
                if (people[index].target == original) {
                    return &people[index].value;
                }
            }
            if (people_count == people.size()) {
                return nullptr;
            }
            people[people_count] = {original, *original};
            return &people[people_count++].value;
        }
        Error link(PersonId id, PersonId other, std::size_t record, std::size_t PersonRecord::*head) noexcept {
            if (relation_count == added_relations.size() || !store.link_room(contact_count + relation_count + 1)) {
                return Error::Capacity;
            }
            auto* person = edit(id);
            if (!person) {
                return Error::WorkLimit;
            }
            added_relations[relation_count] = {other, record, person->*head};
            person->*head = store.relations.size() + relation_count++;
            return Error::None;
        }
        Error connect(PersonId first, PersonId second) noexcept {
            if (contact_count + 2 > added_contacts.size() || !store.link_room(contact_count + relation_count + 2)) {
                return Error::Capacity;
            }
            auto* left = edit(first);
            auto* right = edit(second);
            if (!left || !right) {
                return Error::WorkLimit;
            }
            added_contacts[contact_count] = {second, left->contacts, 0};
            left->contacts = store.links.size() + contact_count++;
            added_contacts[contact_count] = {first, right->contacts, 0};
            right->contacts = store.links.size() + contact_count++;
            return Error::None;
        }
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
            auto* edited = edit(recipient);
            if (!edited) {
                return Error::WorkLimit;
            }
            if (!edited->member || edited->born > order.time || (edited->died && *edited->died <= order.time)) {
                return Error::Life;
            }
            auto& person = *edited;
            if (person.account_count == std::numeric_limits<std::uint64_t>::max()) {
                return Error::Capacity;
            }
            const auto local = ++person.account_count;
            accounts[account_count] = {{recipient, local}, {local, fact, attribution, evidence, order.time}, source, person.accounts};
            person.accounts = store.accounts.rows.size() + account_count++;
            return Error::None;
        }
        void publish() noexcept {
            if (added_person) {
                store.people.append(*added_person);
                store.origins.append({added_person->origin});
                store.creations.append({added_person->address});
                store.person_high = added_person->id.value;
            }
            if (added_partnership) {
                store.partnerships.append(*added_partnership);
            }
            for (std::size_t index = 0; index < partnership_count; ++index) {
                *partnerships[index].target = partnerships[index].value;
            }
            if (membership) {
                *membership->target = membership->value;
            }
            if (added_membership) {
                store.memberships.append(*added_membership);
            }
            for (std::size_t index = 0; index < contact_count; ++index) {
                store.links.push_back(added_contacts[index]);
            }
            for (std::size_t index = 0; index < relation_count; ++index) {
                store.relations.push_back(added_relations[index]);
            }
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
            store.receipts.append({{actor, result.attempt}, body, result});
            store.last = order;
            count(store.counters.records_touched, promise_count + people_count + account_count + due_count +
                (object ? 1 : 0) + (contact ? 1 : 0) + (added_promise ? 1 : 0) + (added_due ? 1 : 0) +
                (added_person ? 3 : 0) + (added_partnership ? 1 : 0) + partnership_count +
                (membership ? 1 : 0) + (added_membership ? 1 : 0) + contact_count + relation_count + 2);
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
    Error prepare(Prepared&, const ProposePartnership&) noexcept;
    Error prepare(Prepared&, const DecidePartnership&) noexcept;
    Error prepare(Prepared&, const EndPartnership&) noexcept;
    Error prepare(Prepared&, const SetMembership&) noexcept;
    Error prepare(Prepared&, const Introduce&) noexcept;
    Error prepare(Prepared&, const Birth&) noexcept;
    Error prepare(Prepared&, const Death&) noexcept;
    Error eligible(const Storage&, const PartnershipEligibility&) noexcept;
}
