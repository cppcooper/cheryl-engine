#pragma once

#include <cheryl/story/actions.h>
#include <algorithm>
#include <type_traits>
#include <vector>

namespace CE::Story::Detail {
    inline constexpr std::size_t absent = std::numeric_limits<std::size_t>::max();
    inline void count(std::uint64_t& value, std::uint64_t amount = 1) noexcept {
        value = amount > std::numeric_limits<std::uint64_t>::max() - value
            ? std::numeric_limits<std::uint64_t>::max() : value + amount;
    }
    inline std::uint64_t mix(std::uint64_t value) noexcept {
        value ^= value >> 30;
        value *= 0xbf58476d1ce4e5b9ULL;
        value ^= value >> 27;
        value *= 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }
    template<class Tag> std::uint64_t hash(Ref<Tag> id) noexcept { return mix(id.value); }
    inline std::uint64_t hash(Origin key) noexcept { return mix(key.timeline.value) ^ mix(key.identity); }
    inline std::uint64_t hash(GenerationAddress key) noexcept { return mix(key.family) ^ mix(key.creation); }
    template<class Key> struct Identity { Key id{}; };
    struct AttemptKey {
        PersonId actor{};
        std::uint64_t attempt{};
        auto operator<=>(const AttemptKey&) const = default;
    };
    inline std::uint64_t hash(AttemptKey key) noexcept { return mix(key.actor.value) ^ mix(key.attempt); }
    struct AccountKey {
        PersonId actor{};
        std::uint64_t local{};
        auto operator<=>(const AccountKey&) const = default;
    };
    inline std::uint64_t hash(AccountKey key) noexcept { return mix(key.actor.value) ^ mix(key.local); }

    template<class Record> struct Table {
        using Key = decltype(Record{}.id);
        std::vector<Record> rows;
        std::vector<std::size_t> buckets;
        std::size_t limit{};

        explicit Table(std::size_t capacity) : buckets(capacity * 2 + 1, absent), limit(capacity) {
            static_assert(std::is_nothrow_copy_constructible_v<Record>);
            static_assert(std::is_nothrow_copy_assignable_v<Record>);
            rows.reserve(capacity);
        }
        std::size_t slot(Key key) const noexcept {
            auto bucket = hash(key) % buckets.size();
            for (std::size_t checked = 0; checked < buckets.size(); ++checked) {
                const auto index = buckets[bucket];
                if (index == absent || rows[index].id == key) {
                    return index;
                }
                bucket = (bucket + 1) % buckets.size();
            }
            return absent;
        }
        Record* find(Key key) noexcept {
            const auto index = slot(key);
            return index == absent ? nullptr : &rows[index];
        }
        const Record* find(Key key) const noexcept {
            const auto index = slot(key);
            return index == absent ? nullptr : &rows[index];
        }
        bool room(std::size_t amount = 1) const noexcept { return amount <= limit - rows.size(); }
        void append(const Record& record) noexcept {
            auto bucket = hash(record.id) % buckets.size();
            while (buckets[bucket] != absent) {
                bucket = (bucket + 1) % buckets.size();
            }
            buckets[bucket] = rows.size();
            rows.push_back(record);
        }
    };
    struct PersonRecord : Person {
        std::size_t contacts{absent};
        std::size_t accounts{absent};
        std::uint64_t account_count{};
        std::uint64_t retired_through{};
        std::uint64_t attempt_high{};
        std::size_t children{absent};
        std::size_t partnerships{absent};
        std::size_t obligations{absent};
    };
    struct ObjectRecord : ObjectState {
        std::size_t promises{absent};
    };
    struct CommitmentRecord : Commitment {
        std::size_t next{absent};
    };
    struct Account {
        AccountKey id{};
        Knowledge value{};
        EpisodeId source{};
        std::size_t next{absent};
    };
    struct Receipt {
        AttemptKey id{};
        std::variant<Request, LifeRequest> body{};
        Result result{};
    };
    struct ContactLink {
        PersonId other{};
        std::size_t next{absent};
        std::int16_t attitude{};
    };
    struct DueWork {
        CommitmentId promise{};
        StoryTime due{};
        bool active{};
    };
    struct RelationLink {
        PersonId other{};
        std::size_t record{absent};
        std::size_t next{absent};
    };
    struct Storage {
        TimelineId timeline;
        StoryTime seeded_at;
        Limits limits;
        std::uint64_t fence{};
        Table<PersonRecord> people;
        Table<ObjectRecord> objects;
        Table<CommitmentRecord> promises;
        Table<Episode> episodes;
        Table<Account> accounts;
        Table<Receipt> receipts;
        Table<Identity<Origin>> origins;
        Table<Identity<GenerationAddress>> creations;
        Table<Partnership> partnerships;
        Table<Membership> memberships;
        std::vector<ContactLink> links;
        std::vector<RelationLink> relations;
        std::vector<DueWork> due;
        std::optional<EventOrder> last;
        Counters counters;
        std::uint64_t person_high{};
        std::uint64_t life_retired{};
        std::uint64_t policy_version{1};

        Storage(TimelineId scope, StoryTime now, Limits caps, std::uint64_t epoch)
            : timeline(scope), seeded_at(now), limits(caps), fence(epoch), people(caps.people), objects(caps.objects),
              promises(caps.commitments), episodes(caps.episodes), accounts(caps.acquisitions), receipts(caps.receipts),
              origins(caps.people), creations(caps.people), partnerships(caps.partnerships), memberships(caps.memberships) {
            links.reserve(caps.links);
            relations.reserve(caps.links);
            due.reserve(caps.queue);
        }
        bool scoped(auto id) const noexcept { return id && id.timeline == timeline; }
        bool alive(PersonId id, StoryTime now) const noexcept {
            const auto* record = people.find(id);
            return living(id, now) && record->member;
        }
        bool living(PersonId id, StoryTime now) const noexcept {
            const auto* record = people.find(id);
            return scoped(id) && record && record->born <= now && (!record->died || now < *record->died);
        }
        bool link_room(std::size_t amount) const noexcept {
            return amount <= limits.links - links.size() - relations.size();
        }
    };
    Error validate_limits(const Limits&) noexcept;
    Error seed(Storage&, const Seed&);
}
