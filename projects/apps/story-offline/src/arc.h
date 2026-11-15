#pragma once

#include "payoff.h"

#include <array>
#include <expected>
#include <iosfwd>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace OfflineStory::Arc {
    enum class Role { Need, Evidence, Renegotiation, Commitment, Settlement, Response };
    enum class Action { NoticeNeed, ShareEvidence, ReviseTerms, PromiseOrRefuse, CheckHandoff, Respond };
    enum class Link { Continue, Present, Withhold, Revised, Promise, Refuse, Deliver, Break, Leave };
    enum class ActorRole { Recipient, Witness };
    enum class Goal { Access, Shelter };
    inline constexpr unsigned need_exists = 1, evidence_available = 2;
    inline constexpr unsigned evidence_known = 1, revised_terms = 2, outcome_known = 4;
    inline constexpr unsigned everyday = 1, mature = 2;

    struct Scope {
        unsigned themes = everyday, explicitness = 0;
        bool adult_language = false;
        unsigned minimum_echelon = 0, maximum_echelon = 3;
        bool operator==(const Scope&) const = default;
    };
    struct Actor {
        std::string id, location;
        ActorRole role = ActorRole::Recipient;
        unsigned information = 0; // Initially only evidence_known is supported; no invented prior settlement.
        bool operator==(const Actor&) const = default;
    };
    struct Society {
        std::string id, speaker_register;
        unsigned echelon = 0;
        bool operator==(const Society&) const = default;
    };
    struct Presentation {
        std::string interface_locale = "en";
        unsigned exposure = 5, player_proficiency = 0;
        bool assistance = false;
        bool operator==(const Presentation&) const = default;
    };
    struct Context {
        Goal goal = Goal::Access;
        std::string recipient, location, material;
        std::vector<Actor> actors;
        unsigned facts = need_exists | evidence_available;
        Society society;
        Presentation presentation; // Retained separately; does not supply facts, membership or action authority.
        unsigned allowed_themes = everyday, maximum_explicitness = 0;
        bool allow_adult_language = false;
        bool operator==(const Context&) const = default;
    };
    struct Component {
        std::string id, actor, recipient, location, material, society, text;
        Role role = Role::Need;
        Action action = Action::NoticeNeed;
        unsigned goals = 3; // Host-approved access/shelter goal bits, not a prose-derived assertion.
        unsigned required_facts = 0, required_information = 0;
        int ordinary_due = 0, revised_due = 0; // Only Commitment may declare these quantities.
        Scope scope;
        bool operator==(const Component&) const = default;
    };
    struct Edge {
        std::size_t from, to;
        Link link;
        bool operator==(const Edge&) const = default;
    };
    struct Draft {
        std::vector<Component> nodes;
        std::vector<Edge> edges;
        bool operator==(const Draft&) const = default;
    };
    struct Limits {
        std::size_t nodes = 6, edges = 9, actors = 2, traces = 8, steps = 64;
        std::size_t text_length = 1024, text_bytes = 32768, storage_bytes = 256 * 1024, work = 100000;
    };
    inline constexpr Limits maximum_limits{};
    enum class Code {
        InvalidLimits, CountLimit, TextLimit, StorageLimit, WorkLimit, Identity, Reference,
        Role, Action, Compatibility, Information, Scope, Cycle, Topology
    };
    struct Error {
        Code code;
        std::string field;
    };
    struct ShapeNode {
        Role role;
        Action action;
        unsigned required_facts, required_information;
        bool operator==(const ShapeNode&) const = default;
    };
    struct ShapeEdge {
        Role from, to;
        Link link;
        bool operator==(const ShapeEdge&) const = default;
    };
    struct Shape {
        std::vector<ShapeNode> nodes;
        std::vector<ShapeEdge> edges;
        bool operator==(const Shape&) const = default;
    };
    struct ComponentMechanics {
        Role role;
        unsigned goals;
        int ordinary_due, revised_due;
        Scope scope;
        bool operator==(const ComponentMechanics&) const = default;
    };
    struct Mechanics {
        Goal goal;
        unsigned facts, echelon;
        std::array<unsigned, 2> initial_information{}; // Logical Recipient/Witness slots, never cosmetic IDs.
        std::vector<ComponentMechanics> components;
        bool operator==(const Mechanics&) const = default;
    };
    struct Trace {
        std::vector<Role> roles;
        std::vector<Link> choices;
        Obligation proposed_obligation = Obligation::None;
        int recorded_quantity = 0, remaining_due = 0;
        unsigned recipient_information = 0;
        bool proposed_favor = false;
        bool operator==(const Trace&) const = default;
    }; // Conditional design preview only. Even a Deliver trace does not perform or certify a live handoff.

    class Artifact;
    using Handle = std::shared_ptr<const Artifact>;
    std::expected<Handle, Error> accept(const Draft&, const Context&, const Limits& = {});
    std::expected<Handle, Error> compose_direct(const std::vector<Component>&, const Context&, const Limits& = {});
    std::expected<Handle, Error> introduce_evidence(const Artifact&, const Component&, const Component&, const Limits& = {});
    int run(std::ostream&); // Private registered caller; proposed arcs and a separate existing live-host handoff witness.

    class Artifact final {
        Draft draft_;
        Context context_;
        Shape shape_;
        Mechanics mechanics_;
        std::vector<Trace> traces_;
        Usage usage_;
        Artifact(Draft, Context, Shape, Mechanics, std::vector<Trace>, Usage);
        friend struct Builder;
    public:
        Artifact(const Artifact&) = delete;
        const Draft& draft() const noexcept { return draft_; }
        const Context& context() const noexcept { return context_; }
        const Shape& shape() const noexcept { return shape_; }
        const Mechanics& mechanics() const noexcept { return mechanics_; }
        const std::vector<Trace>& traces() const noexcept { return traces_; }
        Usage usage() const noexcept { return usage_; }
    };
    // Unique typed roles make canonical ordering exact for this acyclic grammar, not general graph isomorphism.
    // No graph execution, durable-world writes, translation or proficiency assessment is provided by this module.
}
