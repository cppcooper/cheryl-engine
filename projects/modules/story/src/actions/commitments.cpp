#include "prepared.h"

namespace CE::Story::Detail {
    Error prepare(Prepared& change, const OfferPromise& action) noexcept {
        auto& store = change.store;
        if (!store.scoped(action.object) || !store.scoped(action.creditor)) {
            return Error::ForeignReference;
        }
        auto* object = store.objects.find(action.object);
        count(store.counters.records_read);
        if (!object) {
            return Error::Missing;
        }
        if (object->version != action.object_version) {
            return Error::Stale;
        }
        if (object->holder != change.actor) {
            return Error::NotHolder;
        }
        if (action.creditor == change.actor || !store.alive(action.creditor, change.order.time)) {
            return Error::Life;
        }
        if (action.due < change.order.time) {
            return Error::Terms;
        }
        if (!store.promises.room() || store.due.size() == store.limits.queue) {
            return Error::Capacity;
        }
        CommitmentRecord promise;
        promise.id = {store.timeline, store.promises.rows.size() + 1};
        promise.debtor = change.actor;
        promise.creditor = action.creditor;
        promise.object = action.object;
        promise.due = action.due;
        promise.source = change.episode.id;
        promise.next = object->promises;
        auto updated = *object;
        updated.promises = store.promises.rows.size();
        change.object = Replacement<ObjectRecord>{object, updated};
        change.added_promise = promise;
        change.added_due = DueWork{promise.id, promise.due, true};
        auto error = change.link(promise.debtor, promise.creditor, store.promises.rows.size(), &PersonRecord::obligations);
        if (error != Error::None) {
            return error;
        }
        error = change.link(promise.creditor, promise.debtor, store.promises.rows.size(), &PersonRecord::obligations);
        if (error != Error::None) {
            return error;
        }
        change.result.promise = promise.id;
        change.episode.fact = {EpisodeKind::PromiseOffered, change.actor, action.creditor, action.object, promise.id};
        change.episode.audience[1] = action.creditor;
        return Error::None;
    }
    Error prepare(Prepared& change, const DecidePromise& action) noexcept {
        Error error{};
        auto* promise = change.promise(action.promise, action.version, error);
        if (!promise) {
            return error;
        }
        if (promise->creditor != change.actor) {
            return Error::Authority;
        }
        if (promise->state != CommitmentState::Offered) {
            return Error::Terminal;
        }
        if (!change.store.alive(promise->debtor, change.order.time) || change.order.time > promise->due) {
            return Error::Life;
        }
        auto value = *promise;
        value.state = action.accept ? CommitmentState::Pending : CommitmentState::Declined;
        if (!action.accept) {
            value.settlement = change.episode.id;
            error = change.settle_due(value.id);
            if (error != Error::None) {
                return error;
            }
        }
        change.result.state = value.state;
        change.episode.fact = {action.accept ? EpisodeKind::PromiseAccepted : EpisodeKind::PromiseDeclined,
            promise->debtor, promise->creditor, promise->object, promise->id};
        change.episode.support = promise->source;
        change.episode.audience[1] = promise->debtor;
        return change.replace(promise, value);
    }
    Error prepare(Prepared& change, const RequestRelease& action) noexcept {
        Error error{};
        auto* promise = change.promise(action.promise, action.version, error);
        if (!promise) {
            return error;
        }
        if (promise->debtor != change.actor) {
            return Error::Authority;
        }
        if (promise->state != CommitmentState::Pending || promise->release_requested) {
            return Error::Terminal;
        }
        auto value = *promise;
        value.release_requested = true;
        change.episode.fact = {EpisodeKind::ReleaseRequested, promise->debtor, promise->creditor, promise->object, promise->id};
        change.episode.support = promise->source;
        change.episode.audience[1] = promise->creditor;
        return change.replace(promise, value);
    }
    Error prepare(Prepared& change, const DecideRelease& action) noexcept {
        Error error{};
        auto* promise = change.promise(action.promise, action.version, error);
        if (!promise) {
            return error;
        }
        if (promise->creditor != change.actor) {
            return Error::Authority;
        }
        if (promise->state != CommitmentState::Pending || !promise->release_requested) {
            return Error::Terms;
        }
        auto value = *promise;
        value.release_requested = false;
        if (action.accept) {
            value.state = CommitmentState::Released;
            value.settlement = change.episode.id;
            error = change.settle_due(value.id);
            if (error != Error::None) {
                return error;
            }
        }
        change.result.state = value.state;
        change.episode.fact = {action.accept ? EpisodeKind::ReleaseAccepted : EpisodeKind::ReleaseDeclined,
            promise->debtor, promise->creditor, promise->object, promise->id};
        change.episode.support = promise->source;
        change.episode.audience[1] = promise->debtor;
        return change.replace(promise, value);
    }
    Error prepare(Prepared& change, const CheckDeadline& action) noexcept {
        Error error{};
        auto* promise = change.promise(action.promise, action.version, error);
        if (!promise) {
            return error;
        }
        if (change.actor != promise->debtor && change.actor != promise->creditor) {
            return Error::Authority;
        }
        if (promise->state != CommitmentState::Pending || change.order.time <= promise->due) {
            return Error::Terms;
        }
        auto value = *promise;
        value.state = CommitmentState::Breached;
        value.settlement = change.episode.id;
        error = change.settle_due(value.id);
        if (error != Error::None) {
            return error;
        }
        change.result.state = value.state;
        change.episode.fact = {EpisodeKind::Deadline, promise->debtor, promise->creditor, promise->object, promise->id};
        change.episode.support = promise->source;
        change.episode.audience = {promise->debtor, promise->creditor};
        return change.replace(promise, value);
    }
}
