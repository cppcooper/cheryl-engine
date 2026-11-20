#pragma once

#include "types.h"
#include <memory>

namespace CE::Story {
    namespace Detail { struct ActorState; struct ActorAccess; }
    template<class Tag> class ActorRef {
        std::uint64_t scope_{};
        std::uint32_t local_{};
        friend struct Detail::ActorAccess;
    public:
        ActorRef() = default;
        auto operator<=>(const ActorRef&) const = default;
        explicit operator bool() const { return scope_ != 0 && local_ != 0; }
    };
    using KnownPersonRef = ActorRef<struct ActorPersonTag>;
    using KnownObjectRef = ActorRef<struct ActorObjectTag>;
    using KnownPromiseRef = ActorRef<struct ActorPromiseTag>;
    using KnownPartnershipRef = ActorRef<struct ActorPartnershipTag>;
    using AccountRef = ActorRef<struct ActorAccountTag>;
    using OfferRef = ActorRef<struct ActorOfferTag>;
    enum class ActorFactKind { PromiseOffered, PromiseAccepted, PromiseDeclined, ReleaseRequested,
        ReleaseAccepted, ReleaseDeclined, Deadline, TransferConsented, Transferred, Perceived, Told, Response,
        Introduced, PartnershipProposed, PartnershipAccepted, PartnershipDeclined, PartnershipEnded,
        Born, Died, Left, Rejoined, CommitmentFailed, PartnershipCancelled };
    enum class ActorEvidence { Observed, Attributed };
    struct ActorFact {
        ActorFactKind kind{};
        KnownPersonRef subject{};
        KnownPersonRef other{};
        KnownObjectRef object{};
        KnownPromiseRef promise{};
        KnownPartnershipRef partnership{};
        std::optional<StoryTime> due;
        auto operator<=>(const ActorFact&) const = default;
    };
    struct ActorAccount {
        AccountRef reference{};
        ActorFact fact{};
        KnownPersonRef attribution{};
        ActorEvidence evidence{};
        StoryTime acquired{};
        auto operator<=>(const ActorAccount&) const = default;
    };
    struct KnownPerson {
        KnownPersonRef reference{};
        Text label{};
        bool identified{};
        auto operator<=>(const KnownPerson&) const = default;
    };
    struct KnownObject {
        KnownObjectRef reference{};
        Text label{};
        bool identified{};
        auto operator<=>(const KnownObject&) const = default;
    };
    enum class IntentKind { OfferPromise, DecidePromise, RequestRelease, DecideRelease,
        ConsentTransfer, Transfer, Tell, Respond };
    struct ActorIntent {
        IntentKind kind{};
        KnownPersonRef person{};
        KnownObjectRef object{};
        KnownPromiseRef promise{};
        AccountRef account{};
        StoryTime due{};
        bool accept{};
        std::int8_t attitude_delta{};
        auto operator<=>(const ActorIntent&) const = default;
    };
    struct ActorOffer {
        OfferRef reference{};
        ActorIntent intent{};
        Text label{};
        auto operator<=>(const ActorOffer&) const = default;
    };
    struct ActorView {
        StoryTime now{};
        KnownPersonRef self{};
        std::array<KnownPerson, 128> people{};
        std::size_t person_count{};
        std::array<KnownObject, 128> objects{};
        std::size_t object_count{};
        std::array<ActorAccount, 64> accounts{};
        std::size_t account_count{};
        std::size_t cursor{};
        std::size_t total_accounts{};
        std::array<ActorOffer, 32> offers{};
        std::size_t offer_count{};
        auto operator<=>(const ActorView&) const = default;
    };
    struct ActorAttemptKey {
        std::uint64_t value{};
        auto operator<=>(const ActorAttemptKey&) const = default;
    };
    struct ActorChoice {
        OfferRef offer{};
        ActorIntent intent{};
        auto operator<=>(const ActorChoice&) const = default;
    };
    enum class ActorOutcome { Pending, Completed, NotCompleted };
    struct ActorReceipt {
        ActorAttemptKey attempt{};
        ActorOutcome outcome{};
        KnownPromiseRef promise{};
        auto operator<=>(const ActorReceipt&) const = default;
    };
    enum class ActorError { Expired, Invalid, Conflict, Unknown, Busy, Limit, Unavailable };
    class ActorEndpoint {
        std::weak_ptr<Detail::ActorState> state_;
        friend struct Detail::ActorAccess;
    public:
        ActorEndpoint() = default;
        std::expected<ActorView, ActorError> view(std::size_t cursor = 0, std::size_t limit = 64) const;
        std::expected<ActorReceipt, ActorError> submit(ActorAttemptKey, const ActorChoice&) const;
        std::expected<ActorReceipt, ActorError> lookup(ActorAttemptKey) const;
        std::expected<void, ActorError> retire_through(ActorAttemptKey) const;
    };
}
