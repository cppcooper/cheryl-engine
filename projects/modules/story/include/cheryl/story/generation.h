#pragma once

#include "actions.h"
#include <vector>

namespace CE::Story {
    struct VersionPins {
        std::uint32_t schema{1};
        std::uint32_t content{};
        std::uint32_t policy{};
        std::uint32_t generator{};
        std::uint32_t randomness{};
        std::uint32_t selection_mapping{};
        auto operator<=>(const VersionPins&) const = default;
    };
    enum class RandomStream { Life, Decision, Wording };
    struct DecisionAddress {
        RandomStream stream{RandomStream::Decision};
        GenerationAddress subject{};
        std::uint32_t kind{};
        std::uint64_t occurrence{};
        std::optional<GenerationAddress> candidate;
        auto operator<=>(const DecisionAddress&) const = default;
    };
    struct DecisionCursor {
        DecisionAddress address;
        std::uint64_t next_draw{};
        std::optional<GenerationAddress> candidate_frontier;
        auto operator<=>(const DecisionCursor&) const = default;
    };
    struct ChoiceLimits {
        std::size_t candidates_per_decision{32};
        std::size_t draws_per_decision{64};
        std::size_t decisions_per_run{256};
        std::size_t cursors{128};
        auto operator<=>(const ChoiceLimits&) const = default;
    };
    struct ChoiceCounters {
        std::uint64_t decisions{};
        std::uint64_t draws{};
        std::uint64_t candidates_examined{};
        std::uint64_t rejected{};
        auto operator<=>(const ChoiceCounters&) const = default;
    };
    enum class GenerationStatus { Complete, Terminated, HorizonReachedShortfall, PausedByBudget, InvalidInput, Unsupported, Conflict };
    struct ScheduledIntent {
        EventOrder order{};
        PersonId actor{};
        Action action{};
        GenerationAddress address{};
        DecisionAddress decision;
        std::uint64_t expected_version{};
    };
    struct GenerationSpec {
        VersionPins versions;
        std::optional<std::uint64_t> seed;
        std::uint64_t seed_namespace{};
        GenerationAddress lineage{};
        std::uint32_t generation_count{1};
        StoryTime end_time{};
        Limits limits;
        ChoiceLimits choices;
        auto operator<=>(const GenerationSpec&) const = default;
    };
    struct Continuation {
        GenerationSpec specification;
        TimelineId timeline{};
        EventOrder frontier{};
        std::uint64_t accepted_sequence{};
        std::vector<ScheduledIntent> pending;
        std::vector<DecisionCursor> decisions;
        ChoiceCounters choice_work;
    };
    struct GenerationResult {
        GenerationStatus status{GenerationStatus::Unsupported};
        std::uint32_t requested_depth{};
        std::optional<std::uint32_t> achieved_depth;
        Continuation continuation;
        Counters counters;
    };
}
