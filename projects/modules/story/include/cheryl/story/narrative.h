#pragma once

#include "generation.h"

namespace CE::Story {
    enum class PortKind { AcceptedPromise, VerifiedTransfer, AttributedAccount, FulfilledPromise, ReleasedPromise };
    struct NarrativeInput {
        PersonId audience{};
        std::vector<Knowledge> acquired;
        std::size_t work_budget{};
        std::size_t output_budget{};
    };
    struct MeaningUnit {
        PortKind kind{};
        Knowledge evidence{};
        Text wording{};
    };
    struct NarrativeFrontier {
        std::uint32_t definition_revision{};
        PersonId audience{};
        std::vector<Knowledge> supports;
        std::optional<Request> pending;
    };
    struct NarrativeOutcome {
        Result disposition;
        std::vector<MeaningUnit> meaning;
        bool complete{};
    };
}
