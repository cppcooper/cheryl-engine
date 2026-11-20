#pragma once

#include <cheryl/story/actor.h>
#include <cheryl/story/narrative.h>

namespace ActorConsumer {
    namespace Story = CE::Story;
    struct Trace {
        Story::ActorView before{};
        Story::ActorChoice choice{};
        Story::ActorReceipt pending{};
        auto operator<=>(const Trace&) const = default;
    };
    std::expected<Trace, Story::ActorError> choose_response(const Story::ActorEndpoint&, Story::ActorAttemptKey);
    std::expected<Story::NarrativeOutcome, Story::ActorError> observe(const Story::ActorEndpoint&, Story::ActorAttemptKey);
}
