#pragma once

#include "actions.h"
#include <vector>

namespace CE::Story {
    struct VersionPins {
        std::uint32_t schema{1};
        std::uint32_t content{};
        std::uint32_t policy{};
        std::uint32_t randomness{};
        auto operator<=>(const VersionPins&) const = default;
    };
    enum class GenerationStatus { Complete, Terminated, HorizonReachedShortfall, PausedByBudget, InvalidInput, Unsupported, Conflict };
    struct ScheduledIntent {
        EventOrder order{};
        PersonId actor{};
        Action action{};
        GenerationAddress address{};
        std::uint64_t expected_version{};
    };
    struct GenerationSpec {
        VersionPins versions;
        std::uint64_t seed{};
        std::uint32_t generation_count{1};
        StoryTime end_time{};
        Limits limits;
    };
    struct Continuation {
        VersionPins versions;
        TimelineId timeline{};
        EventOrder frontier{};
        std::uint64_t accepted_sequence{};
        std::vector<ScheduledIntent> pending;
    };
    struct GenerationResult {
        GenerationStatus status{GenerationStatus::Unsupported};
        std::uint32_t requested_depth{};
        std::optional<std::uint32_t> achieved_depth;
        Continuation continuation;
        Counters counters;
    };
}
