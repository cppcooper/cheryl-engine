#pragma once

#include "story.h"

namespace OfflineStory {
    enum class FragmentKind { Greeting, Wording, Reply };
    struct Fragment {
        std::string id;
        FragmentKind kind = FragmentKind::Reply;
        std::array<std::string, 2> variants;
    };
    struct WatchOutcome {
        std::string id, operation, condition;
        Method prior = Method::Decline;
        Knowledge learned = Knowledge::None;
        int attitude = 0;
    };
    struct WatchRecipe {
        std::string id, actor, action, outcome, location, wording, reply;
        Method prior = Method::Decline;
    };
    struct FragmentLibrary {
        Origin origin = Origin::Manual;
        std::string id;
        std::uint64_t revision = 1;
        std::vector<Fragment> fragments;
        std::vector<WatchOutcome> outcomes;
        std::vector<WatchRecipe> recipes;
        std::string doubt, known, ashore, shore_known, withhold_word, withhold_reply, ask_word, known_reply, finish_word, finish_reply;
    };
    struct WatchLimits {
        std::size_t fragments = 24, outcomes = 3, recipes = 3, candidates = 3, nodes = 5, choices = 4;
        std::size_t text_length = 1024, text_bytes = 32768, storage_bytes = 256 * 1024, work = 1024 * 1024;
    };
    inline constexpr WatchLimits maximum_watch_limits{};
    struct WatchState {
        State world;
        bool evidence_available = true; // Host-owned live availability, not a claim from prose.
        bool operator==(const WatchState&) const = default;
    };
    struct WatchCheckpoint {
        D::Snapshot dialogue;
        WatchState state;
        bool operator==(const WatchCheckpoint&) const = default;
    };
    class Continuation;
    using ContinuationHandle = std::shared_ptr<const Continuation>;
    std::expected<ContinuationHandle, Error>
    compose_watch(const FragmentLibrary&, ProjectionHandle, const Checkpoint&, std::size_t flavour, const WatchLimits& = {});
    class Continuation final {
        FragmentLibrary library_;
        ProjectionHandle ferry_;
        Checkpoint context_;
        D::DefinitionHandle definition_;
        std::size_t recipe_, outcome_, flavour_;
        Usage usage_;
        Continuation(
            const FragmentLibrary&,
            ProjectionHandle,
            const Checkpoint&,
            D::DefinitionHandle,
            std::size_t,
            std::size_t,
            std::size_t,
            Usage
        );
        friend std::expected<ContinuationHandle, Error>
        compose_watch(const FragmentLibrary&, ProjectionHandle, const Checkpoint&, std::size_t, const WatchLimits&);
    public:
        Continuation(const Continuation&) = delete;
        Continuation& operator=(const Continuation&) = delete;
        const FragmentLibrary& library() const noexcept { return library_; }
        const ProjectionHandle& ferry() const noexcept { return ferry_; }
        const Checkpoint& context() const noexcept { return context_; }
        const D::DefinitionHandle& definition() const noexcept { return definition_; }
        const WatchRecipe& recipe() const noexcept { return library_.recipes[recipe_]; }
        const WatchOutcome& outcome() const noexcept { return library_.outcomes[outcome_]; }
        Usage usage() const noexcept { return usage_; }
        std::size_t flavour() const noexcept { return flavour_; }
    };
    class WatchEncounter {
        ContinuationHandle content_;
        D::Session session_;
        WatchState state_;
        std::uint64_t effects_ = 0;
        WatchEncounter(ContinuationHandle, D::Session);
        bool coherent(const WatchState&) const noexcept;
        bool informed() const noexcept;
        bool test(const D::ConditionKey&) const;
    public:
        WatchEncounter(WatchEncounter&&) noexcept = default;
        WatchEncounter& operator=(WatchEncounter&&) noexcept = default;
        static std::expected<WatchEncounter, Error> start(ContinuationHandle, D::SessionId);
        std::expected<D::View, D::SessionError> view();
        std::expected<D::Transition, D::SessionError> choose(const D::Selection&);
        std::expected<WatchCheckpoint, D::SessionError> capture();
        std::expected<void, Error> restore(const WatchCheckpoint&);
        bool evidence(bool available) noexcept;
        const WatchState& state() const noexcept { return state_; }
        std::uint64_t effects() const noexcept { return effects_; }
    };
    FragmentLibrary manual_library();
    int run_watch(
        std::ostream&,
        std::string_view set,
        std::string_view ferry_action,
        std::string_view action,
        std::size_t flavour,
        std::string_view mode
    );
}
