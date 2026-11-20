#include "prepared.h"

namespace CE::Story {
    Result Family::apply(const Capability& capability, const Request& request) {
        auto& store = *storage_;
        if (capability.owner_ != this || capability.fence_ != store.fence) {
            return {Error::Authority, request.attempt};
        }
        auto* actor = store.people.find(capability.actor_);
        if (!actor || request.attempt == 0 || request.attempt <= actor->retired_through) {
            return {Error::Expired, request.attempt};
        }
        if (const auto* receipt = store.receipts.find({actor->id, request.attempt})) {
            Detail::count(store.counters.retries);
            return receipt->body == request ? receipt->result : Result{Error::Conflict, request.attempt};
        }
        Detail::count(store.counters.preparations);
        if (!store.receipts.room()) {
            return {Error::Capacity, request.attempt};
        }
        Detail::Prepared prepared(store, actor->id, request);
        if (request.order.time < store.seeded_at || (store.last && request.order <= *store.last)) {
            prepared.result.error = Error::Time;
        } else if (!store.alive(actor->id, request.order.time)) {
            prepared.result.error = Error::Life;
        } else if (!store.episodes.room()) {
            prepared.result.error = Error::Capacity;
        } else {
            prepared.result.error = std::visit([&](const auto& action) { return Detail::prepare(prepared, action); }, request.action);
        }
        if (prepared.result.error == Error::None && !std::holds_alternative<Tell>(request.action) &&
            !std::holds_alternative<Perceive>(request.action)) {
            for (const auto recipient : prepared.episode.audience) {
                if (recipient && prepared.result.error == Error::None) {
                    prepared.result.error = prepared.acquire(recipient, prepared.episode.fact, Evidence::Observed, {}, prepared.episode.id);
                }
            }
        }
        if (prepared.result.error != Error::None) {
            store.receipts.append({{actor->id, request.attempt}, request, prepared.result});
            return prepared.result;
        }
        prepared.publish();
        return prepared.result;
    }
}
