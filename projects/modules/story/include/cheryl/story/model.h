#pragma once

#include "types.h"
#include <memory>
#include <span>

namespace CE::Story {
    enum class Lineage { Founder, Incoming, Descendant };
    struct Person {
        PersonId id{};
        Origin origin{};
        GenerationAddress address{};
        Text name{};
        StoryTime born{};
        std::optional<StoryTime> died;
        bool member{true};
        Lineage lineage{Lineage::Incoming};
        std::array<PersonId, 2> parents{};
        std::optional<std::uint32_t> depth;
        std::uint64_t version{1};
    };
    struct ObjectState {
        ObjectId id{};
        PersonId holder{};
        PersonId accepted_recipient{};
        PersonId accepted_holder{};
        std::uint64_t version{1};
        EpisodeId last_transfer{};
    };
    enum class CommitmentState { Offered, Pending, Declined, Fulfilled, Released, Breached, Failed };
    struct Commitment {
        CommitmentId id{};
        PersonId debtor{};
        PersonId creditor{};
        ObjectId object{};
        StoryTime due{};
        CommitmentState state{CommitmentState::Offered};
        bool release_requested{};
        std::uint64_t version{1};
        EpisodeId source{};
        EpisodeId settlement{};
    };
    enum class EpisodeKind { PromiseOffered, PromiseAccepted, PromiseDeclined, ReleaseRequested,
        ReleaseAccepted, ReleaseDeclined, Deadline, TransferConsented, Transferred, Perceived, Told, Response };
    struct Proposition {
        EpisodeKind kind{};
        PersonId subject{};
        PersonId other{};
        ObjectId object{};
        CommitmentId commitment{};
        auto operator<=>(const Proposition&) const = default;
    };
    struct Episode {
        EpisodeId id{};
        EventOrder order{};
        Proposition fact{};
        EpisodeId support{};
        std::array<PersonId, 2> audience{};
    };
    enum class Evidence { Observed, Attributed };
    struct Knowledge {
        std::uint64_t local_sequence{};
        Proposition proposition{};
        PersonId attributed_to{};
        Evidence evidence{};
        StoryTime acquired{};
        auto operator<=>(const Knowledge&) const = default;
    };
    struct Contact {
        PersonId first{};
        PersonId second{};
        auto operator<=>(const Contact&) const = default;
    };
    struct Seed {
        TimelineId timeline{};
        StoryTime now{};
        std::span<const Person> people;
        std::span<const ObjectState> objects;
        std::span<const Contact> contacts;
    };
    class Capability {
        const void* owner_{};
        std::uint64_t fence_{};
        PersonId actor_{};
        friend class Family;
    public:
        Capability() = default;
    };
    struct Request;
    struct Result;
    namespace Detail { struct Storage; }
    class Family {
        std::unique_ptr<Detail::Storage> storage_;
        explicit Family(std::unique_ptr<Detail::Storage> storage);
    public:
        ~Family();
        Family(const Family&) = delete;
        Family& operator=(const Family&) = delete;
        static std::expected<std::unique_ptr<Family>, Error> create(const Seed&, Limits = {});
        Capability actor(PersonId) const;
        Result apply(const Capability&, const Request&);
        Error retire_through(const Capability&, std::uint64_t attempt);
        std::optional<Person> person(PersonId) const;
        std::optional<ObjectState> object(ObjectId) const;
        std::optional<Commitment> commitment(CommitmentId) const;
        std::optional<Episode> episode(EpisodeId) const;
        std::expected<std::size_t, Error> knowledge(const Capability&, std::span<Knowledge>) const;
        Counters counters() const;
        std::size_t episode_count() const;
        std::size_t acquisition_count() const;
    };
}
