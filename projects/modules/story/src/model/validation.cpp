#include "../actions/prepared.h"

namespace CE::Story::Detail {
    Error validate_limits(const Limits& caps) noexcept {
        std::size_t bytes = sizeof(Storage) + sizeof(Family) + sizeof(Prepared);
        const auto charge = [&](std::size_t count, std::size_t size) {
            if (count > (std::numeric_limits<std::size_t>::max() - 1) / 2 ||
                count > (caps.bytes - std::min(bytes, caps.bytes)) / size) {
                return false;
            }
            bytes += count * size;
            return bytes <= caps.bytes;
        };
        if (bytes > caps.bytes || caps.people == 0 || caps.episodes == 0 || caps.receipts == 0 || caps.work_per_action == 0) {
            return Error::Capacity;
        }
        if (!charge(caps.people, sizeof(PersonRecord) + 2 * sizeof(std::size_t)) ||
            !charge(caps.objects, sizeof(ObjectRecord) + 2 * sizeof(std::size_t)) ||
            !charge(caps.commitments, sizeof(CommitmentRecord) + 2 * sizeof(std::size_t)) ||
            !charge(caps.episodes, sizeof(Episode) + 2 * sizeof(std::size_t)) ||
            !charge(caps.acquisitions, sizeof(Account) + 2 * sizeof(std::size_t)) ||
            !charge(caps.receipts, sizeof(Receipt) + 2 * sizeof(std::size_t)) ||
            !charge(caps.people, sizeof(Identity<Origin>) + sizeof(Identity<GenerationAddress>) + 4 * sizeof(std::size_t)) ||
            !charge(caps.partnerships, sizeof(Partnership) + 2 * sizeof(std::size_t)) ||
            !charge(caps.memberships, sizeof(Membership) + 2 * sizeof(std::size_t)) ||
            !charge(10, sizeof(std::size_t)) || !charge(caps.links, sizeof(ContactLink) + sizeof(RelationLink) + sizeof(Contact)) ||
            !charge(caps.queue, sizeof(DueWork)) ||
            !charge(caps.people, sizeof(std::uint8_t) + sizeof(std::size_t))) {
            return Error::Bytes;
        }
        return Error::None;
    }

    Error seed(Storage& store, const Seed& input) {
        if (input.timeline.value == 0 || input.life_policy_version == 0 || input.people.empty() || input.people.size() > store.limits.people ||
            input.objects.size() > store.limits.objects || input.contacts.size() > store.limits.links / 2) {
            return Error::InvalidSeed;
        }
        store.policy_version = input.life_policy_version;
        for (const auto& person : input.people) {
            if (!store.scoped(person.id)) {
                return Error::ForeignReference;
            }
            if (store.people.find(person.id)) {
                return Error::Duplicate;
            }
            if (person.name.size > person.name.bytes.size() || person.born > input.now ||
                (person.died && (*person.died < person.born || *person.died > input.now)) || person.version != 1 ||
                person.origin.timeline.value == 0 || person.origin.identity == 0 || person.address.family == 0 ||
                person.address.creation == 0 ||
                (person.lineage != Lineage::Founder && person.lineage != Lineage::Incoming &&
                    person.lineage != Lineage::Descendant) || person.source != EpisodeId{} ||
                person.birth_support != PartnershipId{} || person.membership != MembershipId{}) {
                return Error::InvalidSeed;
            }
            PersonRecord record;
            static_cast<Person&>(record) = person;
            record.depth.reset();
            record.introduced = input.now;
            store.people.append(record);
            store.person_high = std::max(store.person_high, person.id.value);
        }
        std::vector<std::uint8_t> marks(input.people.size());
        std::vector<std::size_t> pending;
        pending.reserve(input.people.size());
        for (std::size_t index = 0; index < store.people.rows.size(); ++index) {
            pending.push_back(index);
        }
        const auto unique = [&](auto identity) {
            std::sort(pending.begin(), pending.end(), [&](auto left, auto right) {
                return identity(store.people.rows[left]) < identity(store.people.rows[right]);
            });
            return std::adjacent_find(pending.begin(), pending.end(), [&](auto left, auto right) {
                return identity(store.people.rows[left]) == identity(store.people.rows[right]);
            }) == pending.end();
        };
        if (!unique([](const auto& person) { return person.origin; }) ||
            !unique([](const auto& person) { return person.address; })) {
            return Error::Duplicate;
        }
        pending.clear();
        for (auto& person : store.people.rows) {
            store.origins.append({person.origin});
            store.creations.append({person.address});
            if (person.member && !person.died) {
                if (!store.memberships.room()) {
                    return Error::Capacity;
                }
                person.membership = {store.timeline, store.memberships.rows.size() + 1};
                store.memberships.append({person.membership, person.id, input.now, {}, {}, {}, {}});
            }
        }
        bool founder = false;
        for (std::size_t index = 0; index < store.people.rows.size(); ++index) {
            auto& person = store.people.rows[index];
            if ((person.lineage != Lineage::Descendant && (person.parents[0] || person.parents[1])) ||
                (person.lineage == Lineage::Descendant && !person.parents[0] && !person.parents[1]) ||
                (person.parents[0] && person.parents[0] == person.parents[1])) {
                return Error::InvalidSeed;
            }
            for (const auto parent : person.parents) {
                if (!parent) {
                    if (parent != PersonId{}) {
                        return Error::ForeignReference;
                    }
                    continue;
                }
                if (!store.scoped(parent)) {
                    return Error::ForeignReference;
                }
                const auto* ancestor = store.people.find(parent);
                if (!ancestor) {
                    return Error::Missing;
                }
                if (parent == person.id) {
                    return Error::Cycle;
                }
            }
            if (person.lineage == Lineage::Founder) {
                founder = true;
            }
        }
        for (std::size_t start = 0; start < store.people.rows.size(); ++start) {
            if (marks[start] == 2) {
                continue;
            }
            pending.push_back(start);
            while (!pending.empty()) {
                const auto index = pending.back();
                auto& person = store.people.rows[index];
                marks[index] = 1;
                bool ready = true;
                std::optional<std::uint32_t> depth;
                for (const auto parent : person.parents) {
                    if (!parent) {
                        continue;
                    }
                    const auto parent_index = store.people.slot(parent);
                    if (marks[parent_index] == 1) {
                        return Error::Cycle;
                    }
                    if (marks[parent_index] == 0) {
                        pending.push_back(parent_index);
                        ready = false;
                        break;
                    }
                    const auto parent_depth = store.people.rows[parent_index].depth;
                    if (parent_depth) {
                        depth = std::max(depth.value_or(0), *parent_depth);
                    }
                }
                if (!ready) {
                    continue;
                }
                if (person.lineage == Lineage::Founder) {
                    person.depth = 0;
                } else if (person.lineage == Lineage::Descendant) {
                    if (!depth || *depth == std::numeric_limits<std::uint32_t>::max()) {
                        return Error::Depth;
                    }
                    person.depth = *depth + 1;
                }
                marks[index] = 2;
                pending.pop_back();
            }
        }
        if (!founder) {
            return Error::Depth;
        }
        for (const auto& person : store.people.rows) {
            for (const auto parent : person.parents) {
                if (!parent) {
                    continue;
                }
                const auto* ancestor = store.people.find(parent);
                if (ancestor->born >= person.born || (ancestor->died && *ancestor->died <= person.born)) {
                    return Error::Life;
                }
                if (!store.link_room(1)) {
                    return Error::Capacity;
                }
                auto* mutable_parent = store.people.find(parent);
                store.relations.push_back({person.id, absent, mutable_parent->children});
                mutable_parent->children = store.relations.size() - 1;
            }
        }
        for (const auto& object : input.objects) {
            if (!store.scoped(object.id) || !store.scoped(object.holder)) {
                return Error::ForeignReference;
            }
            if (!store.people.find(object.holder)) {
                return Error::Missing;
            }
            if (store.objects.find(object.id)) {
                return Error::Duplicate;
            }
            if (object.accepted_recipient != PersonId{} || object.accepted_holder != PersonId{} ||
                object.last_transfer != EpisodeId{} || object.version != 1) {
                return Error::InvalidSeed;
            }
            ObjectRecord record;
            static_cast<ObjectState&>(record) = object;
            store.objects.append(record);
        }
        std::vector<Contact> contacts(input.contacts.begin(), input.contacts.end());
        for (auto& contact : contacts) {
            if (contact.second < contact.first) {
                std::swap(contact.first, contact.second);
            }
        }
        std::sort(contacts.begin(), contacts.end(), [&](const auto& left, const auto& right) {
            count(store.counters.import_contact_comparisons);
            return left < right;
        });
        for (std::size_t index = 1; index < contacts.size(); ++index) {
            count(store.counters.import_contact_comparisons);
            if (contacts[index - 1] == contacts[index]) {
                return Error::Duplicate;
            }
        }
        for (const auto& contact : contacts) {
            if (!store.scoped(contact.first) || !store.scoped(contact.second)) {
                return Error::ForeignReference;
            }
            auto* first = store.people.find(contact.first);
            auto* second = store.people.find(contact.second);
            if (!first || !second) {
                return Error::Missing;
            }
            if (first == second) {
                return Error::InvalidSeed;
            }
            if (!store.link_room(2)) {
                return Error::Capacity;
            }
            store.links.push_back({second->id, first->contacts, 0});
            first->contacts = store.links.size() - 1;
            store.links.push_back({first->id, second->contacts, 0});
            second->contacts = store.links.size() - 1;
        }
        count(store.counters.validation_passes);
        return Error::None;
    }
}
