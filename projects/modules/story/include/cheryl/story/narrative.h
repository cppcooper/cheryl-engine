#pragma once

#include "actor.h"
#include <vector>

namespace CE::Story {
    enum class PortKind { AcceptedPromise, VerifiedTransfer, AttributedAccount, FulfilledPromise, ReleasedPromise };
    struct NarrativeInput {
        KnownPersonRef audience{};
        std::vector<ActorAccount> acquired;
        std::size_t work_budget{};
        std::size_t output_budget{};
    };
    struct MeaningUnit {
        PortKind kind{};
        ActorAccount evidence{};
        Text wording{};
    };
    struct NarrativeFrontier {
        std::uint32_t definition_revision{};
        KnownPersonRef audience{};
        std::vector<ActorAccount> supports;
        std::optional<ActorChoice> pending;
    };
    struct NarrativeOutcome {
        ActorReceipt disposition;
        std::vector<MeaningUnit> meaning;
        bool complete{};
    };
}
