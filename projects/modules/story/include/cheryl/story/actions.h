#pragma once

#include "model.h"
#include <variant>

namespace CE::Story {
    struct OfferPromise {
        PersonId creditor{};
        ObjectId object{};
        StoryTime due{};
        std::uint64_t object_version{};
        auto operator<=>(const OfferPromise&) const = default;
    };
    struct DecidePromise {
        CommitmentId promise{};
        std::uint64_t version{};
        bool accept{};
        auto operator<=>(const DecidePromise&) const = default;
    };
    struct RequestRelease {
        CommitmentId promise{};
        std::uint64_t version{};
        auto operator<=>(const RequestRelease&) const = default;
    };
    struct DecideRelease {
        CommitmentId promise{};
        std::uint64_t version{};
        bool accept{};
        auto operator<=>(const DecideRelease&) const = default;
    };
    struct CheckDeadline {
        CommitmentId promise{};
        std::uint64_t version{};
        auto operator<=>(const CheckDeadline&) const = default;
    };
    struct ConsentTransfer {
        ObjectId object{};
        PersonId holder{};
        std::uint64_t version{};
        auto operator<=>(const ConsentTransfer&) const = default;
    };
    struct Transfer {
        ObjectId object{};
        PersonId recipient{};
        std::uint64_t version{};
        auto operator<=>(const Transfer&) const = default;
    };
    struct Perceive {
        EpisodeId incident{};
        auto operator<=>(const Perceive&) const = default;
    };
    struct Tell {
        PersonId recipient{};
        std::uint64_t account{};
        auto operator<=>(const Tell&) const = default;
    };
    struct Respond {
        PersonId contact{};
        std::uint64_t account{};
        std::int8_t attitude_delta{};
        auto operator<=>(const Respond&) const = default;
    };
    struct ProposePartnership {
        PartnershipPermit permit{};
        auto operator<=>(const ProposePartnership&) const = default;
    };
    struct DecidePartnership {
        PartnershipId partnership{};
        std::uint64_t version{};
        bool accept{};
        auto operator<=>(const DecidePartnership&) const = default;
    };
    struct EndPartnership {
        PartnershipId partnership{};
        std::uint64_t version{};
        auto operator<=>(const EndPartnership&) const = default;
    };
    struct SetMembership {
        std::uint64_t version{};
        bool join{};
        auto operator<=>(const SetMembership&) const = default;
    };
    using Action = std::variant<OfferPromise, DecidePromise, RequestRelease, DecideRelease, CheckDeadline,
        ConsentTransfer, Transfer, Perceive, Tell, Respond, ProposePartnership, DecidePartnership,
        EndPartnership, SetMembership>;
    struct Request {
        std::uint64_t attempt{};
        EventOrder order{};
        Action action{};
        auto operator<=>(const Request&) const = default;
    };
    struct Result {
        Error error{Error::None};
        std::uint64_t attempt{};
        CommitmentId promise{};
        CommitmentState state{CommitmentState::Offered};
        PersonId person{};
        PartnershipId partnership{};
        EpisodeId episode{};
        auto operator<=>(const Result&) const = default;
        explicit operator bool() const { return error == Error::None; }
    };
    struct NewPerson {
        Origin origin{};
        GenerationAddress address{};
        Text name{};
        auto operator<=>(const NewPerson&) const = default;
    };
    struct Introduce {
        NewPerson person{};
        StoryTime born{};
        PersonId contact{};
        std::uint64_t contact_version{};
        auto operator<=>(const Introduce&) const = default;
    };
    struct Birth {
        NewPerson person{};
        PartnershipId partnership{};
        std::uint64_t partnership_version{};
        std::array<std::uint64_t, 2> parent_versions{};
        std::int64_t minimum_parent_age{};
        bool allowed{};
        auto operator<=>(const Birth&) const = default;
    };
    struct Death {
        PersonId person{};
        std::uint64_t version{};
        auto operator<=>(const Death&) const = default;
    };
    using LifeAction = std::variant<Introduce, Birth, Death>;
    struct LifeRequest {
        std::uint64_t attempt{};
        EventOrder order{};
        std::uint64_t policy_version{};
        LifeAction action{};
        auto operator<=>(const LifeRequest&) const = default;
    };
}
