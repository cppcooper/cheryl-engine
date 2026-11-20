#include "prepared.h"

namespace CE::Story {
    namespace {
        Result finish(Detail::Prepared& prepared, bool observed) noexcept {
            auto& store = prepared.store;
            if (prepared.result.error == Error::None && observed) {
                for (const auto recipient : prepared.episode.audience) {
                    if (recipient && prepared.result.error == Error::None) {
                        prepared.result.error = prepared.acquire(recipient, prepared.episode.fact,
                            Evidence::Observed, {}, prepared.episode.id);
                    }
                }
            }
            if (prepared.result.error != Error::None) {
                prepared.result = {prepared.result.error, prepared.result.attempt};
                store.receipts.append({{prepared.actor, prepared.result.attempt}, prepared.body, prepared.result});
                Detail::count(store.counters.records_touched);
                return prepared.result;
            }
            prepared.result.episode = prepared.episode.id;
            prepared.publish();
            return prepared.result;
        }
    }
    Result Family::apply(const Capability& capability, const Request& request) {
        auto& store = *storage_;
        if (capability.owner_ != this || capability.fence_ != store.fence) {
            return {Error::Authority, request.attempt};
        }
        auto* actor = store.people.find(capability.actor_);
        if (!actor || request.attempt == 0 || request.attempt <= actor->retired_through) {
            return {Error::Expired, request.attempt};
        }
        actor->attempt_high = std::max(actor->attempt_high, request.attempt);
        if (const auto* receipt = store.receipts.find({actor->id, request.attempt})) {
            Detail::count(store.counters.retries);
            const auto* prior = std::get_if<Request>(&receipt->body);
            return prior && *prior == request ? receipt->result : Result{Error::Conflict, request.attempt};
        }
        Detail::count(store.counters.preparations);
        if (!store.receipts.room()) {
            return {Error::Capacity, request.attempt};
        }
        Detail::Prepared prepared(store, actor->id, request);
        if (request.order.time < store.seeded_at || (store.last && request.order <= *store.last)) {
            prepared.result.error = Error::Time;
        } else if (!store.living(actor->id, request.order.time) ||
            (!actor->member && !std::holds_alternative<SetMembership>(request.action))) {
            prepared.result.error = Error::Life;
        } else if (!store.episodes.room()) {
            prepared.result.error = Error::Capacity;
        } else {
            const auto* proposal = std::get_if<ProposePartnership>(&request.action);
            if (proposal && (proposal->permit.owner_ != this || proposal->permit.fence_ != store.fence)) {
                prepared.result.error = Error::Authority;
            } else {
                prepared.result.error = std::visit([&](const auto& action) { return Detail::prepare(prepared, action); }, request.action);
            }
        }
        return finish(prepared, !std::holds_alternative<Tell>(request.action) && !std::holds_alternative<Perceive>(request.action));
    }
    Result Family::apply(const LifeCapability& capability, const LifeRequest& request) {
        auto& store = *storage_;
        if (capability.owner_ != this || capability.fence_ != store.fence) {
            return {Error::Authority, request.attempt};
        }
        if (request.attempt == 0 || request.attempt <= store.life_retired) {
            return {Error::Expired, request.attempt};
        }
        if (const auto* receipt = store.receipts.find({{}, request.attempt})) {
            Detail::count(store.counters.retries);
            const auto* prior = std::get_if<LifeRequest>(&receipt->body);
            return prior && *prior == request ? receipt->result : Result{Error::Conflict, request.attempt};
        }
        Detail::count(store.counters.preparations);
        if (!store.receipts.room()) {
            return {Error::Capacity, request.attempt};
        }
        Detail::Prepared prepared(store, {}, request);
        if (request.policy_version != store.policy_version) {
            prepared.result.error = Error::Policy;
        } else if (request.order.time < store.seeded_at || (store.last && request.order <= *store.last)) {
            prepared.result.error = Error::Time;
        } else if (!store.episodes.room()) {
            prepared.result.error = Error::Capacity;
        } else {
            prepared.result.error = std::visit([&](const auto& action) { return Detail::prepare(prepared, action); }, request.action);
        }
        return finish(prepared, true);
    }
}
