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
        StoryTime introduced{};
        EpisodeId source{};
        PartnershipId birth_support{};
        MembershipId membership{};
    };
    enum class PartnershipState { Proposed, Active, Declined, Ended, Cancelled };
    enum class PartnershipEnding { EitherMayWithdraw };
    struct Partnership {
        PartnershipId id{};
        PersonId first{};
        PersonId second{};
        PartnershipState state{PartnershipState::Proposed};
        PartnershipEnding ending{PartnershipEnding::EitherMayWithdraw};
        std::uint64_t first_version{};
        std::uint64_t second_version{};
        std::uint64_t policy_version{};
        std::int64_t minimum_age{};
        std::uint64_t version{1};
        EpisodeId proposed{};
        EpisodeId accepted{};
        EpisodeId ended{};
        StoryTime since{};
        std::optional<StoryTime> until;
    };
    struct Membership {
        MembershipId id{};
        PersonId person{};
        StoryTime since{};
        std::optional<StoryTime> until;
        EpisodeId source{};
        EpisodeId ending{};
        MembershipId previous{};
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
        ReleaseAccepted, ReleaseDeclined, Deadline, TransferConsented, Transferred, Perceived, Told, Response,
        Introduced, PartnershipProposed, PartnershipAccepted, PartnershipDeclined, PartnershipEnded,
        Born, Died, Left, Rejoined, CommitmentFailed, PartnershipCancelled };
    struct Proposition {
        EpisodeKind kind{};
        PersonId subject{};
        PersonId other{};
        ObjectId object{};
        CommitmentId commitment{};
        PartnershipId partnership{};
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
        // Imported membership starts at now; no earlier membership history is inferred.
        std::uint64_t life_policy_version{1};
    };
    class Capability {
        const void* owner_{};
        std::uint64_t fence_{};
        PersonId actor_{};
        friend class Family;
    public:
        Capability() = default;
    };
    class LifeCapability {
        const void* owner_{};
        std::uint64_t fence_{};
        friend class Family;
    public:
        LifeCapability() = default;
    };
    struct PartnershipEligibility {
        PersonId first{};
        PersonId second{};
        std::uint64_t first_version{};
        std::uint64_t second_version{};
        std::uint64_t policy_version{};
        StoryTime at{};
        std::int64_t minimum_age{};
        bool adults_allowed{};
        bool kinship_allowed{};
        PartnershipEnding ending{PartnershipEnding::EitherMayWithdraw};
        auto operator<=>(const PartnershipEligibility&) const = default;
    };
    class PartnershipPermit {
        const void* owner_{};
        std::uint64_t fence_{};
        PartnershipEligibility eligibility_{};
        friend class Family;
    public:
        PartnershipPermit() = default;
        const PartnershipEligibility& eligibility() const { return eligibility_; }
        auto operator<=>(const PartnershipPermit&) const = default;
    };
    struct Request;
    struct LifeRequest;
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
        // Raw Family access is trusted host access, not a character facade.
        // Life permits validate host policy assertions; they never imply partner consent.
        LifeCapability life() const;
        std::expected<PartnershipPermit, Error> permit(const LifeCapability&, const PartnershipEligibility&) const;
        Result apply(const Capability&, const Request&);
        // Loss of a party fails offered/pending work, preserves terminal settlements,
        // and closes partnerships. More than 16 affected work items or partnerships
        // refuses atomically. Receipts and due slots retain lifetime capacity usage.
        Result apply(const LifeCapability&, const LifeRequest&);
        Error retire_through(const Capability&, std::uint64_t attempt);
        Error retire_through(const LifeCapability&, std::uint64_t attempt);
        std::optional<Person> person(PersonId) const;
        std::optional<ObjectState> object(ObjectId) const;
        std::optional<Commitment> commitment(CommitmentId) const;
        std::optional<Episode> episode(EpisodeId) const;
        std::optional<Partnership> partnership(PartnershipId) const;
        std::optional<Membership> membership(MembershipId) const;
        std::expected<std::size_t, Error> knowledge(const Capability&, std::span<Knowledge>) const;
        Counters counters() const;
        std::size_t episode_count() const;
        std::size_t acquisition_count() const;
    };
}
