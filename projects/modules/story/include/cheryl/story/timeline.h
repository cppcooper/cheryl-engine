#pragma once

#include "generation.h"

namespace CE::Story {
    struct TimelineCut {
        TimelineId source{};
        std::uint64_t accepted_sequence{};
        EventOrder frontier{};
        VersionPins versions;
    };
    struct RecipientIntervention {
        PersonId debtor{};
        ObjectId object{};
        PersonId alternate{};
        std::uint64_t expected_version{};
    };
    enum class ForkStatus { Accepted, PausedByBudget, Conflict, Unsupported, NoCandidateWithinBudget };
    enum class Correspondence { SharedOrigin, EquivalentCreation, Absent, Unresolved };
    struct NormalizedPerson {
        Origin origin{};
        GenerationAddress address{};
        Correspondence correspondence{Correspondence::Unresolved};
    };
}
