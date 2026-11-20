#include "prepared.h"

namespace CE::Story::Detail {
    namespace {
        Error current(Storage& store, PersonId id, std::uint64_t version, StoryTime at) noexcept {
            if (!store.scoped(id)) {
                return Error::ForeignReference;
            }
            const auto* person = store.people.find(id);
            count(store.counters.records_read);
            if (!person) {
                return Error::Missing;
            }
            if (person->version != version) {
                return Error::Stale;
            }
            return store.alive(id, at) ? Error::None : Error::Life;
        }
        bool old_enough(const Person& person, StoryTime at, std::int64_t age) noexcept {
            const auto eligible_at = StoryTime::add(person.born, age);
            return age >= 0 && eligible_at && *eligible_at <= at;
        }
        Error join(Prepared& change, PersonRecord& person) noexcept {
            if (!change.store.memberships.room()) {
                return Error::Capacity;
            }
            Membership membership;
            membership.id = {change.store.timeline, change.store.memberships.rows.size() + 1};
            membership.person = person.id;
            membership.since = change.order.time;
            membership.source = change.episode.id;
            membership.previous = person.membership;
            person.member = true;
            person.membership = membership.id;
            change.added_membership = membership;
            return Error::None;
        }
        Error create_person(Prepared& change, const NewPerson& input, StoryTime born, Lineage lineage) noexcept {
            auto& store = change.store;
            if (input.origin.timeline.value == 0 || input.origin.identity == 0 || input.address.family == 0 ||
                input.address.creation == 0 || input.name.size > input.name.bytes.size()) {
                return Error::Terms;
            }
            if (store.origins.find(input.origin)) {
                return Error::DuplicateOrigin;
            }
            if (store.creations.find(input.address)) {
                return Error::DuplicateCreation;
            }
            if (!store.people.room() || store.person_high == std::numeric_limits<std::uint64_t>::max()) {
                return Error::Capacity;
            }
            PersonRecord person;
            person.id = {store.timeline, store.person_high + 1};
            person.origin = input.origin;
            person.address = input.address;
            person.name = input.name;
            person.born = born;
            person.introduced = change.order.time;
            person.source = change.episode.id;
            person.lineage = lineage;
            const auto error = join(change, person);
            if (error != Error::None) {
                return error;
            }
            change.added_person = person;
            change.result.person = person.id;
            return Error::None;
        }
        Partnership* partnership(Prepared& change, PartnershipId id, std::uint64_t version, Error& error) noexcept {
            auto& store = change.store;
            if (!store.scoped(id)) {
                error = Error::ForeignReference;
                return nullptr;
            }
            auto* record = store.partnerships.find(id);
            count(store.counters.records_read);
            if (!record) {
                error = Error::Missing;
            } else if (change.actor != record->first && change.actor != record->second) {
                error = Error::Authority;
            } else if (record->version != version) {
                error = Error::Stale;
            } else if (record->version == std::numeric_limits<std::uint64_t>::max()) {
                error = Error::Capacity;
            } else {
                return record;
            }
            return nullptr;
        }
        Error replace_partnership(Prepared& change, Partnership* target, Partnership value) noexcept {
            if (change.partnership_count == change.partnerships.size()) {
                return Error::WorkLimit;
            }
            if (value.version == std::numeric_limits<std::uint64_t>::max()) {
                return Error::Capacity;
            }
            ++value.version;
            change.partnerships[change.partnership_count++] = {target, value};
            return Error::None;
        }
        Error notify(Prepared& change, PersonId recipient, Proposition fact) noexcept {
            if (!change.store.alive(recipient, change.order.time)) {
                return Error::None;
            }
            return change.acquire(recipient, fact, Evidence::Observed, {}, change.episode.id);
        }
        Error lose_party(Prepared& change, PersonRecord& person) noexcept {
            auto& store = change.store;
            // Selected policy: absence/death cancels offered work and fails pending work,
            // not voluntary breach. Terminal settlements and acquired accounts survive.
            for (auto index = person.obligations; index != absent; index = store.relations[index].next) {
                if (!change.step()) {
                    return Error::WorkLimit;
                }
                count(store.counters.links_traversed);
                auto& promise = store.promises.rows[store.relations[index].record];
                count(store.counters.records_read);
                if (promise.state != CommitmentState::Offered && promise.state != CommitmentState::Pending) {
                    continue;
                }
                if (promise.version == std::numeric_limits<std::uint64_t>::max()) {
                    return Error::Capacity;
                }
                auto value = promise;
                value.state = CommitmentState::Failed;
                value.release_requested = false;
                value.settlement = change.episode.id;
                auto error = change.replace(&promise, value);
                if (error == Error::None) {
                    error = change.settle_due(promise.id);
                }
                if (error == Error::None) {
                    error = notify(change, store.relations[index].other,
                        {EpisodeKind::CommitmentFailed, promise.debtor, promise.creditor, promise.object, promise.id});
                }
                if (error != Error::None) {
                    return error;
                }
            }
            for (auto index = person.partnerships; index != absent; index = store.relations[index].next) {
                if (!change.step()) {
                    return Error::WorkLimit;
                }
                count(store.counters.links_traversed);
                auto& record = store.partnerships.rows[store.relations[index].record];
                count(store.counters.records_read);
                if (record.state != PartnershipState::Proposed && record.state != PartnershipState::Active) {
                    continue;
                }
                auto value = record;
                value.state = record.state == PartnershipState::Proposed ? PartnershipState::Cancelled : PartnershipState::Ended;
                value.ended = change.episode.id;
                value.until = change.order.time;
                auto error = replace_partnership(change, &record, value);
                if (error == Error::None) {
                    error = notify(change, store.relations[index].other,
                        {record.state == PartnershipState::Proposed ? EpisodeKind::PartnershipCancelled : EpisodeKind::PartnershipEnded,
                            record.first, record.second, {}, {}, record.id});
                }
                if (error != Error::None) {
                    return error;
                }
            }
            if (auto* membership = store.memberships.find(person.membership); membership && !membership->until) {
                auto value = *membership;
                value.until = change.order.time;
                value.ending = change.episode.id;
                change.membership = Replacement<Membership>{membership, value};
            }
            person.member = false;
            return Error::None;
        }
    }

    Error eligible(const Storage& store, const PartnershipEligibility& input) noexcept {
        if (input.policy_version != store.policy_version || !input.adults_allowed || !input.kinship_allowed ||
            input.minimum_age < 0 || input.ending != PartnershipEnding::EitherMayWithdraw) {
            return Error::Policy;
        }
        if (!store.scoped(input.first) || !store.scoped(input.second)) {
            return Error::ForeignReference;
        }
        const auto* first = store.people.find(input.first);
        const auto* second = store.people.find(input.second);
        if (!first || !second) {
            return Error::Missing;
        }
        if (first->version != input.first_version || second->version != input.second_version) {
            return Error::Stale;
        }
        if (first == second || !store.alive(first->id, input.at) || !store.alive(second->id, input.at) ||
            !old_enough(*first, input.at, input.minimum_age) || !old_enough(*second, input.at, input.minimum_age)) {
            return Error::Life;
        }
        return Error::None;
    }
    Error prepare(Prepared& change, const Introduce& action) noexcept {
        if (action.born > change.order.time) {
            return Error::Life;
        }
        if (action.contact != PersonId{}) {
            const auto error = current(change.store, action.contact, action.contact_version, change.order.time);
            if (error != Error::None) {
                return error;
            }
        } else if (action.contact_version != 0) {
            return Error::Terms;
        }
        auto error = create_person(change, action.person, action.born, Lineage::Incoming);
        if (error != Error::None) {
            return error;
        }
        if (action.contact) {
            error = change.connect(change.result.person, action.contact);
        }
        change.episode.fact = {EpisodeKind::Introduced, change.result.person, action.contact, {}, {}};
        change.episode.audience = {action.contact, {}};
        return error;
    }
    Error prepare(Prepared& change, const ProposePartnership& action) noexcept {
        auto& store = change.store;
        const auto& input = action.permit.eligibility();
        if (input.first != change.actor) {
            return Error::Authority;
        }
        if (input.at != change.order.time) {
            return Error::Time;
        }
        auto error = eligible(store, input);
        count(store.counters.records_read, 2);
        if (error != Error::None) {
            return error;
        }
        if (!store.partnerships.room()) {
            return Error::Capacity;
        }
        const auto* person = store.people.find(input.first);
        for (auto index = person->partnerships; index != absent; index = store.relations[index].next) {
            if (!change.step()) {
                return Error::WorkLimit;
            }
            count(store.counters.links_traversed);
            const auto& record = store.partnerships.rows[store.relations[index].record];
            count(store.counters.records_read);
            if (store.relations[index].other == input.second &&
                (record.state == PartnershipState::Proposed || record.state == PartnershipState::Active)) {
                return Error::Terms;
            }
        }
        Partnership record;
        record.id = {store.timeline, store.partnerships.rows.size() + 1};
        record.first = input.first;
        record.second = input.second;
        record.first_version = input.first_version;
        record.second_version = input.second_version;
        record.policy_version = input.policy_version;
        record.minimum_age = input.minimum_age;
        record.ending = input.ending;
        record.proposed = change.episode.id;
        error = change.link(record.first, record.second, store.partnerships.rows.size(), &PersonRecord::partnerships);
        if (error == Error::None) {
            error = change.link(record.second, record.first, store.partnerships.rows.size(), &PersonRecord::partnerships);
        }
        change.added_partnership = record;
        change.result.partnership = record.id;
        change.episode.fact = {EpisodeKind::PartnershipProposed, record.first, record.second, {}, {}, record.id};
        change.episode.audience[1] = record.second;
        return error;
    }
    Error prepare(Prepared& change, const DecidePartnership& action) noexcept {
        Error error{};
        auto* record = partnership(change, action.partnership, action.version, error);
        if (!record) {
            return error;
        }
        if (record->second != change.actor) {
            return Error::Authority;
        }
        if (record->state != PartnershipState::Proposed) {
            return Error::Terminal;
        }
        if (action.accept) {
            error = eligible(change.store, {record->first, record->second, record->first_version, record->second_version,
                record->policy_version, change.order.time, record->minimum_age, true, true, record->ending});
            count(change.store.counters.records_read, 2);
            if (error != Error::None) {
                return error;
            }
            bool connected = false;
            const auto* first = change.store.people.find(record->first);
            for (auto index = first->contacts; index != absent; index = change.store.links[index].next) {
                if (!change.step()) {
                    return Error::WorkLimit;
                }
                count(change.store.counters.links_traversed);
                if (change.store.links[index].other == record->second) {
                    connected = true;
                    break;
                }
            }
            if (!connected) {
                error = change.connect(record->first, record->second);
                if (error != Error::None) {
                    return error;
                }
            }
        }
        auto value = *record;
        value.state = action.accept ? PartnershipState::Active : PartnershipState::Declined;
        if (action.accept) {
            value.accepted = change.episode.id;
            value.since = change.order.time;
        } else {
            value.ended = change.episode.id;
            value.until = change.order.time;
        }
        change.result.partnership = record->id;
        change.episode.fact = {action.accept ? EpisodeKind::PartnershipAccepted : EpisodeKind::PartnershipDeclined,
            record->first, record->second, {}, {}, record->id};
        change.episode.support = record->proposed;
        change.episode.audience = {record->first, record->second};
        return replace_partnership(change, record, value);
    }
    Error prepare(Prepared& change, const EndPartnership& action) noexcept {
        Error error{};
        auto* record = partnership(change, action.partnership, action.version, error);
        if (!record) {
            return error;
        }
        if (record->state != PartnershipState::Active) {
            return Error::Terminal;
        }
        if (record->ending != PartnershipEnding::EitherMayWithdraw) {
            return Error::Policy;
        }
        auto value = *record;
        value.state = PartnershipState::Ended;
        value.until = change.order.time;
        value.ended = change.episode.id;
        change.result.partnership = record->id;
        change.episode.fact = {EpisodeKind::PartnershipEnded, record->first, record->second, {}, {}, record->id};
        change.episode.support = record->accepted;
        change.episode.audience = {record->first, record->second};
        return replace_partnership(change, record, value);
    }
    Error prepare(Prepared& change, const Birth& action) noexcept {
        auto& store = change.store;
        if (!action.allowed || action.minimum_parent_age < 0) {
            return Error::Policy;
        }
        if (!store.scoped(action.partnership)) {
            return Error::ForeignReference;
        }
        const auto* record = store.partnerships.find(action.partnership);
        count(store.counters.records_read);
        if (!record) {
            return Error::Missing;
        }
        if (record->version != action.partnership_version) {
            return Error::Stale;
        }
        if (record->state != PartnershipState::Active || !record->accepted) {
            return Error::Consent;
        }
        if (record->policy_version != store.policy_version) {
            return Error::Policy;
        }
        const std::array parents{record->first, record->second};
        std::optional<std::uint32_t> depth;
        for (std::size_t index = 0; index < parents.size(); ++index) {
            const auto error = current(store, parents[index], action.parent_versions[index], change.order.time);
            if (error != Error::None) {
                return error;
            }
            const auto* person = store.people.find(parents[index]);
            if (person->born >= change.order.time || !old_enough(*person, change.order.time, action.minimum_parent_age)) {
                return Error::Life;
            }
            if (person->depth) {
                depth = std::max(depth.value_or(0), *person->depth);
            }
        }
        if (!depth || *depth == std::numeric_limits<std::uint32_t>::max()) {
            return Error::Depth;
        }
        auto error = create_person(change, action.person, change.order.time, Lineage::Descendant);
        if (error != Error::None) {
            return error;
        }
        auto& child = *change.added_person;
        child.parents = parents;
        child.depth = *depth + 1;
        child.birth_support = record->id;
        for (const auto parent : parents) {
            error = change.link(parent, child.id, absent, &PersonRecord::children);
            if (error == Error::None) {
                error = change.connect(parent, child.id);
            }
            if (error != Error::None) {
                return error;
            }
        }
        change.episode.fact = {EpisodeKind::Born, child.id, {}, {}, {}, record->id};
        change.episode.support = record->accepted;
        change.episode.audience = parents;
        return Error::None;
    }
    Error prepare(Prepared& change, const Death& action) noexcept {
        auto& store = change.store;
        if (!store.scoped(action.person)) {
            return Error::ForeignReference;
        }
        const auto* person = store.people.find(action.person);
        count(store.counters.records_read);
        if (!person) {
            return Error::Missing;
        }
        if (person->version != action.version) {
            return Error::Stale;
        }
        if (!store.living(action.person, change.order.time)) {
            return Error::Life;
        }
        if (person->version == std::numeric_limits<std::uint64_t>::max()) {
            return Error::Capacity;
        }
        auto* value = change.edit(action.person);
        if (!value) {
            return Error::WorkLimit;
        }
        const auto error = lose_party(change, *value);
        value->died = change.order.time;
        ++value->version;
        change.result.person = action.person;
        change.episode.fact = {EpisodeKind::Died, action.person, {}, {}, {}};
        change.episode.audience = {};
        return error;
    }
    Error prepare(Prepared& change, const SetMembership& action) noexcept {
        auto* person = change.edit(change.actor);
        if (!person || !change.store.living(change.actor, change.order.time)) {
            return Error::Life;
        }
        if (person->version != action.version) {
            return Error::Stale;
        }
        if (person->version == std::numeric_limits<std::uint64_t>::max()) {
            return Error::Capacity;
        }
        if (person->member == action.join) {
            return Error::Terminal;
        }
        const auto error = action.join ? join(change, *person) : lose_party(change, *person);
        ++person->version;
        change.result.person = person->id;
        change.episode.fact = {action.join ? EpisodeKind::Rejoined : EpisodeKind::Left, person->id, {}, {}, {}};
        // The departing actor does not acquire a new account while outside scope.
        change.episode.audience = action.join ? std::array<PersonId, 2>{person->id, {}} : std::array<PersonId, 2>{};
        return error;
    }
}
