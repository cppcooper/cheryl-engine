#pragma once

#include "continuation.h"

namespace OfflineStory {
    enum class ReliefLocation { Quay, Bank };
    enum class ReliefIntent { Ask, Help, Pay, Promise, Negotiate, Commit, Refuse };
    enum class ReliefResolution { Open, Helped, Paid, Promised, Negotiated, Refused };
    enum class ReliefKnowledge { None, Discussed, SawHelp, SawPayment, HeardPromise, HeardRefusal };
    enum class ReliefTextRole { Proposition, Wording, Information, Reply, Closing };
    struct ReliefText {
        std::string id;
        ReliefTextRole role = ReliefTextRole::Reply;
        std::array<std::string, 2> flavours;
    };
    struct ReliefAction {
        std::string id, wording, reply, operation, condition;
        ReliefIntent kind = ReliefIntent::Refuse;
        ReliefResolution claimed = ReliefResolution::Refused;
    };
    struct ReliefRequest {
        std::string id, actor, rule, proposition, information, proposal, closing;
        ReliefLocation location = ReliefLocation::Quay;
    };
    struct BargainSets {
        Origin origin = Origin::Manual;
        std::string id;
        std::uint64_t revision = 1;
        std::vector<ReliefText> texts;
        std::vector<ReliefAction> actions;
        std::vector<ReliefRequest> requests;
    };
    struct ReliefActor {
        std::string id, name;
        ReliefLocation location = ReliefLocation::Quay;
    };
    struct ReliefRule {
        std::string id;
        int now_planks = 1, coins = 2, promise_planks = 2, negotiated_planks = 1;
    };
    struct ReliefCatalogue {
        std::string id;
        std::uint64_t revision = 1;
        std::vector<ReliefActor> actors;
        std::vector<ReliefRule> rules;
        std::vector<std::string> operations, conditions;
    };
    struct BargainContext {
        ContinuationHandle previous;
        WatchCheckpoint completed;
    };
    enum class ReliefBundle { OpenOffer, ExplainFirst };
    struct BargainPick {
        std::string request;
        std::size_t flavour = 0;
        ReliefBundle bundle = ReliefBundle::OpenOffer;
        bool include_plan_identity = false; // Seeded projections distinguish independently selected host plans.
    };
    struct BargainLimits {
        std::size_t texts = 40, actions = 7, requests = 8, actors = 2, rules = 4, names = 8, candidates = 8;
        std::size_t nodes = 8, choices = 16, text_length = 1024, text_bytes = 32768, storage_bytes = 256 * 1024, work = 1024 * 1024;
    };
    inline constexpr BargainLimits maximum_bargain_limits{};
    struct ReliefState {
        State world;
        bool terms_known = false;
        ReliefResolution resolution = ReliefResolution::Open;
        int planks_due = 0; // Pending only: this unit has no fulfillment operation/reward.
        ReliefKnowledge requester_knows = ReliefKnowledge::None;
        int requester_attitude = 0;
        bool operator==(const ReliefState&) const = default;
    };
    struct BargainCheckpoint {
        D::Snapshot dialogue;
        ReliefState state;
        bool operator==(const BargainCheckpoint&) const = default;
    };
    class BargainArtifact;
    using BargainHandle = std::shared_ptr<const BargainArtifact>;
    std::expected<BargainHandle, Error>
    compose_bargain(const BargainSets&, const ReliefCatalogue&, const BargainContext&, const BargainPick&, const BargainLimits& = {});
    class BargainArtifact final {
        BargainSets sets_;
        ReliefCatalogue catalogue_;
        BargainContext context_;
        BargainPick pick_;
        D::DefinitionHandle definition_;
        std::size_t request_, rule_;
        Usage usage_;
        BargainArtifact(
            const BargainSets&,
            const ReliefCatalogue&,
            const BargainContext&,
            const BargainPick&,
            D::DefinitionHandle,
            std::size_t,
            std::size_t,
            Usage
        );
        friend std::expected<BargainHandle, Error>
        compose_bargain(const BargainSets&, const ReliefCatalogue&, const BargainContext&, const BargainPick&, const BargainLimits&);
    public:
        BargainArtifact(const BargainArtifact&) = delete;
        BargainArtifact& operator=(const BargainArtifact&) = delete;
        const BargainSets& sets() const noexcept { return sets_; }
        const ReliefCatalogue& catalogue() const noexcept { return catalogue_; }
        const BargainContext& context() const noexcept { return context_; }
        const BargainPick& pick() const noexcept { return pick_; }
        const D::DefinitionHandle& definition() const noexcept { return definition_; }
        const ReliefRequest& request() const noexcept { return sets_.requests[request_]; }
        const ReliefRule& rule() const noexcept { return catalogue_.rules[rule_]; }
        Usage usage() const noexcept { return usage_; }
    };
    class BargainEncounter {
        BargainHandle content_;
        D::Session session_;
        ReliefState state_;
        std::uint64_t effects_ = 0;
        BargainEncounter(BargainHandle, D::Session);
        bool coherent(const ReliefState&) const noexcept;
        bool eligible(ReliefIntent) const noexcept;
        bool test(const D::ConditionKey&) const;
    public:
        BargainEncounter(BargainEncounter&&) noexcept = default;
        BargainEncounter& operator=(BargainEncounter&&) noexcept = default;
        static std::expected<BargainEncounter, Error> start(BargainHandle, D::SessionId);
        std::expected<D::View, D::SessionError> view();
        std::expected<D::Transition, D::SessionError> choose(const D::Selection&);
        std::expected<BargainCheckpoint, D::SessionError> capture();
        std::expected<void, Error> restore(const BargainCheckpoint&);
        bool resources(int coins, int planks) noexcept;
        const ReliefState& state() const noexcept { return state_; }
        std::uint64_t effects() const noexcept { return effects_; }
    };
    BargainSets manual_bargain_sets();
    ReliefCatalogue relief_catalogue();
    void print_relief(std::ostream&, const ReliefState&);
    int run_bargain(
        std::ostream&,
        std::string_view configuration,
        std::string_view prior,
        std::string_view route,
        std::size_t flavour,
        std::string_view mode
    );
}
