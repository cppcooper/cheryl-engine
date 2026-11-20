#include "prepared.h"

namespace CE::Story::Detail {
    Error prepare(Prepared& change, const ConsentTransfer& action) noexcept {
        auto& store = change.store;
        if (!store.scoped(action.object) || !store.scoped(action.holder)) {
            return Error::ForeignReference;
        }
        auto* object = store.objects.find(action.object);
        count(store.counters.records_read);
        if (!object) {
            return Error::Missing;
        }
        if (object->version != action.version) {
            return Error::Stale;
        }
        if (object->holder != action.holder) {
            return Error::NotHolder;
        }
        if (action.holder == change.actor || !store.alive(action.holder, change.order.time)) {
            return Error::Life;
        }
        if (object->version == std::numeric_limits<std::uint64_t>::max()) {
            return Error::Capacity;
        }
        auto value = *object;
        value.accepted_holder = action.holder;
        value.accepted_recipient = change.actor;
        ++value.version;
        change.object = Replacement<ObjectRecord>{object, value};
        change.episode.fact = {EpisodeKind::TransferConsented, action.holder, change.actor, action.object, {}};
        change.episode.audience[1] = action.holder;
        return Error::None;
    }
    Error prepare(Prepared& change, const Transfer& action) noexcept {
        auto& store = change.store;
        if (!store.scoped(action.object) || !store.scoped(action.recipient)) {
            return Error::ForeignReference;
        }
        auto* object = store.objects.find(action.object);
        count(store.counters.records_read);
        if (!object) {
            return Error::Missing;
        }
        if (object->version != action.version) {
            return Error::Stale;
        }
        if (object->holder != change.actor) {
            return Error::NotHolder;
        }
        if (action.recipient == change.actor || !store.alive(action.recipient, change.order.time)) {
            return Error::Life;
        }
        if (object->accepted_holder != change.actor || object->accepted_recipient != action.recipient) {
            return Error::Consent;
        }
        if (object->version == std::numeric_limits<std::uint64_t>::max()) {
            return Error::Capacity;
        }
        auto value = *object;
        value.holder = action.recipient;
        value.accepted_holder = {};
        value.accepted_recipient = {};
        value.last_transfer = change.episode.id;
        ++value.version;
        change.object = Replacement<ObjectRecord>{object, value};
        change.episode.fact = {EpisodeKind::Transferred, change.actor, action.recipient, action.object, {}};
        change.episode.audience[1] = action.recipient;
        for (auto index = object->promises; index != absent; index = store.promises.rows[index].next) {
            if (!change.step()) {
                return Error::WorkLimit;
            }
            count(store.counters.links_traversed);
            auto& promise = store.promises.rows[index];
            count(store.counters.records_read);
            if (promise.state != CommitmentState::Pending || promise.debtor != change.actor || promise.creditor != action.recipient) {
                continue;
            }
            if (promise.version == std::numeric_limits<std::uint64_t>::max()) {
                return Error::Capacity;
            }
            auto settled = promise;
            settled.state = change.order.time <= promise.due ? CommitmentState::Fulfilled : CommitmentState::Breached;
            settled.settlement = change.episode.id;
            auto error = change.replace(&promise, settled);
            if (error != Error::None) {
                return error;
            }
            error = change.settle_due(promise.id);
            if (error != Error::None) {
                return error;
            }
            change.result.promise = promise.id;
            change.result.state = settled.state;
        }
        return Error::None;
    }
}
