#pragma once

#include "payoff.h"

namespace OfflineStory {
    enum class Reliability { Unknown, Kept, Broken, Declined, Helped, Paid, Pending };
    enum class ReliabilityEvidence { None, Direct, Report };
    enum class ReliabilityBelief { Uncertain, Reliable, Unreliable, Repairing };
    struct ReliabilityObserver {
        Reliability known = Reliability::Unknown;
        ReliabilityEvidence evidence = ReliabilityEvidence::None;
        ReliabilityBelief belief = ReliabilityBelief::Uncertain;
        int trust = 0;
        bool operator==(const ReliabilityObserver&) const = default;
    };
    enum class AftermathBeat { Steward, Ferry, Reported, Reconciled, Booked, Left };
    struct AftermathState {
        State world;
        std::array<ReliabilityObserver, 2> observers; // Nera's direct knowledge, Mara's separately acquired report.
        AftermathBeat beat = AftermathBeat::Steward;
        bool reported = false, reconciled = false, return_passage = false;
        int restitution_paid = 0, fare_paid = 0, settlement_coins = -1;
        bool operator==(const AftermathState&) const = default;
    };
    struct AftermathCheckpoint {
        D::Snapshot dialogue;
        AftermathState state;
        bool operator==(const AftermathCheckpoint&) const = default;
    };
    struct AftermathLimits {
        std::size_t nodes = 6, choices = 11, text_length = 2048, text_bytes = 16384;
        std::size_t storage_bytes = 65536, retained_bytes = 2 * 1024 * 1024, work = 512 * 1024;
    };
    inline constexpr AftermathLimits maximum_aftermath_limits{};
    class AftermathContent;
    std::expected<AftermathContent, Error> compose_aftermath(PayoffHandle, const PayoffCheckpoint&, const AftermathLimits& = {});
    class AftermathContent {
        PayoffHandle previous_;
        PayoffCheckpoint source_;
        D::DefinitionHandle definition_;
        Reliability fact_;
        Usage usage_;
        std::size_t retained_bytes_;
        AftermathContent(PayoffHandle, const PayoffCheckpoint&, D::DefinitionHandle, Reliability, Usage, std::size_t);
        friend std::expected<AftermathContent, Error> compose_aftermath(PayoffHandle, const PayoffCheckpoint&, const AftermathLimits&);
    public:
        const PayoffHandle& previous() const noexcept { return previous_; }
        const PayoffCheckpoint& source() const noexcept { return source_; }
        const D::DefinitionHandle& definition() const noexcept { return definition_; }
        Reliability fact() const noexcept { return fact_; }
        Usage usage() const noexcept { return usage_; }
        std::size_t retained_bytes() const noexcept { return retained_bytes_; }
    };
    class AftermathEncounter {
        AftermathContent content_;
        D::Session session_;
        AftermathState state_;
        std::uint64_t effects_ = 0;
        AftermathEncounter(AftermathContent, D::Session);
        bool coherent(const AftermathState&) const noexcept;
        bool eligible(std::string_view) const noexcept;
    public:
        AftermathEncounter(AftermathEncounter&&) noexcept = default;
        AftermathEncounter& operator=(AftermathEncounter&&) noexcept = default;
        static std::expected<AftermathEncounter, Error> start(AftermathContent, D::SessionId);
        std::expected<D::View, D::SessionError> view();
        std::expected<D::Transition, D::SessionError> choose(const D::Selection&);
        std::expected<AftermathCheckpoint, D::SessionError> capture();
        std::expected<void, Error> restore(const AftermathCheckpoint&);
        bool resources(int coins) noexcept;
        const AftermathContent& content() const noexcept { return content_; }
        const AftermathState& state() const noexcept { return state_; }
        std::uint64_t effects() const noexcept { return effects_; }
        int fare() const noexcept;
    };
}
