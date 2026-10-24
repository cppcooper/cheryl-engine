#pragma once

#include <cheryl/core/game-framework/dialogue.h>

#include <array>
#include <ostream>
#include <string_view>

namespace OfflineStory {
    namespace D = CE::Dialogue;
    enum class Origin { Manual };
    enum class Method { Pay, Repair, Decline };
    enum class Knowledge { None, SawFare, SawRepair, SawDecline, HeardPassage };

    struct Facts {
        int coins = 8, planks = 3;
        bool passage = false, licensed = false, paid = false, repaired = false, declined = false;
        bool operator==(const Facts&) const = default;
    };
    struct State {
        Facts facts;
        std::array<Knowledge, 3> knowledge{};
        std::array<bool, 3> believes_unlicensed{};
        std::array<int, 3> attitude{};
        bool operator==(const State&) const = default;
    };
    struct Context {
        State state;
        std::array<std::string, 3> observers{"mara", "ivo", "lena"};
    };
    struct Consequence {
        int coins = 0, planks = 0; // Nonnegative costs, never narrative rewards.
        bool passage = false, paid = false, repaired = false, declined = false;
        std::array<int, 3> attitude{};
        bool operator==(const Consequence&) const = default;
    };
    struct Outcome {
        std::string id, actor, operation, condition;
        Method method = Method::Decline;
        Consequence consequence;
        std::vector<std::string> rewards;
    };
    struct Actor {
        std::string id, name;
    };
    struct Catalogue {
        std::vector<Actor> actors;
        std::vector<std::string> operations, conditions, rewards;
        std::vector<Outcome> outcomes;
    };
    struct Flavour {
        std::string id, wording, response;
    };
    struct Action {
        std::string id, actor, operation, condition, outcome;
        Consequence claimed;
        std::vector<std::string> rewards;
        std::vector<Flavour> flavours;
    };
    struct CandidateSet {
        Origin origin = Origin::Manual;
        std::string id, proposition;
        std::uint64_t revision = 1;
        std::vector<Action> actions;
    };
    struct Limits {
        std::size_t actors = 8, outcomes = 8, actions = 3, flavours = 4, names = 8, reward_refs = 4;
        std::size_t text_length = 1024, text_bytes = 32768, storage_bytes = 256 * 1024, work = 1024 * 1024;
    };
    inline constexpr Limits maximum_limits{}; // Lower-only private prototype allowances; IDs additionally cap at 64 bytes.
    enum class Code {
        InvalidLimits,
        CountLimit,
        TextLimit,
        StorageLimit,
        WorkLimit,
        Identity,
        Duplicate,
        UnknownActor,
        UnknownOperation,
        UnknownCondition,
        UnknownReward,
        MissingReference,
        Conflict,
        Context,
        Variant,
        EmptyHandle,
        Compile,
        Resume
    };
    struct Error {
        Code code;
        std::string field, value;
        std::size_t row = 0;
        std::vector<D::Diagnostic> compiler;
        std::optional<D::SessionError> session;
    };
    struct Usage {
        std::size_t text_bytes = 0, storage_bytes = 0, work = 0;
    };

    class Accepted;
    using AcceptedHandle = std::shared_ptr<const Accepted>;
    std::expected<AcceptedHandle, Error> accept(const CandidateSet&, const Catalogue&, const Context&, const Limits& = {});
    class Accepted final {
        CandidateSet set_;
        Catalogue catalogue_;
        Context context_;
        Usage usage_;
        Accepted(const CandidateSet&, const Catalogue&, const Context&, Usage);
        friend std::expected<AcceptedHandle, Error> accept(const CandidateSet&, const Catalogue&, const Context&, const Limits&);
    public:
        Accepted(const Accepted&) = delete;
        Accepted& operator=(const Accepted&) = delete;
        const CandidateSet& set() const noexcept { return set_; }
        const Catalogue& catalogue() const noexcept { return catalogue_; }
        const Context& context() const noexcept { return context_; }
        Usage usage() const noexcept { return usage_; }
    };

    class Projection;
    using ProjectionHandle = std::shared_ptr<const Projection>;
    std::expected<ProjectionHandle, Error> project(AcceptedHandle, std::size_t flavour, const Limits& = {});
    class Projection final {
        AcceptedHandle accepted_;
        D::DefinitionHandle definition_;
        std::size_t flavour_;
        Usage usage_;
        Projection(AcceptedHandle, D::DefinitionHandle, std::size_t, Usage);
        friend std::expected<ProjectionHandle, Error> project(AcceptedHandle, std::size_t, const Limits&);
    public:
        Projection(const Projection&) = delete;
        Projection& operator=(const Projection&) = delete;
        const Accepted& accepted() const noexcept { return *accepted_; }
        const D::DefinitionHandle& definition() const noexcept { return definition_; }
        std::size_t flavour() const noexcept { return flavour_; }
        Usage usage() const noexcept { return usage_; }
    };

    struct Checkpoint {
        D::Snapshot dialogue;
        State state;
        bool operator==(const Checkpoint&) const = default;
    };
    class Encounter {
        ProjectionHandle projection_;
        D::Session session_;
        State state_;
        std::uint64_t effects_ = 0; // Unsaved test/demo instrumentation, never a reward ledger.
        Encounter(ProjectionHandle, D::Session);
        const Outcome& rule(std::string_view key, std::string_view prefix) const;
    public:
        Encounter(Encounter&&) noexcept = default;
        Encounter& operator=(Encounter&&) noexcept = default;
        static std::expected<Encounter, Error> start(ProjectionHandle, D::SessionId);
        std::expected<D::View, D::SessionError> view();
        std::expected<D::Transition, D::SessionError> choose(const D::Selection&);
        std::expected<Checkpoint, D::SessionError> capture();
        std::expected<void, Error> restore(const Checkpoint&);
        bool resources(int coins, int planks) noexcept;
        const State& state() const noexcept { return state_; }
        std::uint64_t effects() const noexcept { return effects_; }
    };

    Catalogue ferry_catalogue();
    std::vector<CandidateSet> manual_sets();
    bool valid_state(const State&) noexcept;
    bool eligible(const Outcome&, const State&) noexcept;
    State stage(const Outcome&, const State&);
    void print_state(std::ostream&, const State&, const Accepted&);
    int run(std::ostream&, std::string_view set, std::string_view action, std::size_t flavour, std::string_view mode);
}
