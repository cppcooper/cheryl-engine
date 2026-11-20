#include "consumer.h"

namespace ActorConsumer {
    std::expected<Trace, Story::ActorError> choose_response(const Story::ActorEndpoint& endpoint, Story::ActorAttemptKey key) {
        const auto view = endpoint.view();
        if (!view) { return std::unexpected(view.error()); }
        for (std::size_t index = 0; index < view->offer_count; ++index) {
            const auto& offer = view->offers[index];
            if (offer.intent.kind != Story::IntentKind::Respond) { continue; }
            const Story::ActorChoice choice{offer.reference, offer.intent};
            const auto pending = endpoint.submit(key, choice);
            if (!pending) { return std::unexpected(pending.error()); }
            return Trace{*view, choice, *pending};
        }
        return std::unexpected(Story::ActorError::Unknown);
    }
    std::expected<Story::NarrativeOutcome, Story::ActorError> observe(const Story::ActorEndpoint& endpoint, Story::ActorAttemptKey key) {
        const auto receipt = endpoint.lookup(key);
        if (!receipt) { return std::unexpected(receipt.error()); }
        Story::NarrativeOutcome result;
        result.disposition = *receipt;
        result.complete = receipt->outcome != Story::ActorOutcome::Pending;
        return result;
    }
}
