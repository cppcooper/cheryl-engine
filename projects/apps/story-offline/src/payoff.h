#pragma once

#include "selection.h"

namespace OfflineStory {
    struct PayoffFragment {
        std::string id;
        std::array<std::string, 2> text;
    };
    struct PayoffLibrary {
        Origin origin = Origin::Manual;
        std::string id;
        std::uint64_t revision = 1;
        std::vector<PayoffFragment> fragments;
    };
    struct HandoffEvent {
        ReliefLocation location = ReliefLocation::Quay;
        std::string recipient;
        bool suspected_breach = false; // Explicit unverified host report, not authoritative failure.
        bool operator==(const HandoffEvent&) const = default;
    };
    enum class Obligation { None, Pending, Fulfilled, Broken };
    enum class PayoffOutcome { Open, Acknowledged, Fulfilled, Broken, Left };
    enum class RecipientKnowledge { SawHelp, SawPayment, HeardPromise, HeardRefusal, CheckedDelivery, HeardBreach };
    struct PayoffState {
        State world;
        bool at_handoff = false, encountered = false;
        Obligation obligation = Obligation::None;
        int due = 0, delivered = 0;
        bool favor = false;
        PayoffOutcome outcome = PayoffOutcome::Open;
        RecipientKnowledge recipient_knows = RecipientKnowledge::HeardRefusal;
        int recipient_attitude = 0;
        bool believes_broken = false;
        int settlement_planks = -1; // Pre-resolution balance for coherent scalar accounting, not authenticated history.
        bool operator==(const PayoffState&) const = default;
    };
    struct PayoffCheckpoint {
        D::Snapshot dialogue;
        PayoffState state;
        bool operator==(const PayoffCheckpoint&) const = default;
    };
    struct PayoffLimits {
        std::size_t ingredients = 8, candidates = 4, nodes = 4, choices = 3, text_length = 2048, text_bytes = 16384;
        std::size_t storage_bytes = 64 * 1024, retained_bytes = 1024 * 1024, work = 256 * 1024;
    };
    inline constexpr PayoffLimits maximum_payoff_limits{};
    struct PayoffUsage {
        Usage owned;
        std::size_t retained_bytes = 0;
    };
    class PayoffContent;
    using PayoffHandle = std::shared_ptr<const PayoffContent>;
    std::expected<PayoffHandle, Error>
    compose_payoff(BargainHandle, const BargainCheckpoint&, const HandoffEvent&, const PayoffLibrary&, const PayoffLimits& = {});
    class PayoffContent final {
        BargainHandle bargain_;
        BargainCheckpoint source_;
        HandoffEvent event_;
        PayoffLibrary library_;
        D::DefinitionHandle definition_;
        PayoffUsage usage_;
        PayoffContent(BargainHandle, const BargainCheckpoint&, const HandoffEvent&, const PayoffLibrary&, D::DefinitionHandle, PayoffUsage);
        friend std::expected<PayoffHandle, Error>
        compose_payoff(BargainHandle, const BargainCheckpoint&, const HandoffEvent&, const PayoffLibrary&, const PayoffLimits&);
    public:
        PayoffContent(const PayoffContent&) = delete;
        const BargainHandle& bargain() const noexcept { return bargain_; }
        const BargainCheckpoint& source() const noexcept { return source_; }
        const HandoffEvent& event() const noexcept { return event_; }
        const PayoffLibrary& library() const noexcept { return library_; }
        const D::DefinitionHandle& definition() const noexcept { return definition_; }
        PayoffUsage usage() const noexcept { return usage_; }
    };
    class PayoffEncounter {
        PayoffHandle content_;
        D::Session session_;
        PayoffState state_;
        std::uint64_t effects_ = 0;
        PayoffEncounter(PayoffHandle, D::Session);
        bool coherent(const PayoffState&) const noexcept;
        bool test(const D::ConditionKey&) const;
    public:
        PayoffEncounter(PayoffEncounter&&) noexcept = default;
        PayoffEncounter& operator=(PayoffEncounter&&) noexcept = default;
        static std::expected<PayoffEncounter, Error> start(PayoffHandle, D::SessionId);
        std::expected<D::View, D::SessionError> view();
        std::expected<D::Transition, D::SessionError> choose(const D::Selection&);
        std::expected<PayoffCheckpoint, D::SessionError> capture();
        std::expected<void, Error> restore(const PayoffCheckpoint&);
        std::expected<void, Error> handoff(bool present); // Explicit serialized host presence event; never a Session effect.
        bool resources(int planks) noexcept;              // Serialized host edit while open; conditions recheck this balance.
        const PayoffState& state() const noexcept { return state_; }
        std::uint64_t effects() const noexcept { return effects_; }
    };
    PayoffLibrary manual_payoff_library();
    void print_payoff(std::ostream&, const PayoffState&);
    int run_payoff(std::ostream&, std::uint64_t, std::string_view prior, std::string_view route, std::string_view mode);
}
