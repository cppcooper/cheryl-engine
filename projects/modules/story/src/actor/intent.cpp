#include "state.h"

namespace CE::Story::Detail {
    namespace {
        template<class Tag, class Raw, std::size_t Capacity>
        const Binding<Raw>* resolve(const std::array<Binding<Raw>, Capacity>& bindings,
            std::size_t count, std::uint64_t scope, ActorRef<Tag> reference, bool actionable = true) {
            for (std::size_t index = 0; index < count; ++index) {
                if (ActorAccess::ref<Tag>(scope, index + 1) == reference &&
                    (!actionable || bindings[index].actionable)) { return &bindings[index]; }
            }
            return nullptr;
        }
        std::optional<std::size_t> account_index(const ActorState& state, AccountRef reference) {
            for (std::size_t index = 0; index < state.view.account_count; ++index) {
                if (state.view.accounts[index].reference == reference) { return index; }
            }
            return std::nullopt;
        }
    }
    bool valid(const ActorState& state, const ActorIntent& intent) {
        const auto person = resolve(state.people, state.view_people_count, state.scope, intent.person);
        const auto object = resolve(state.objects, state.view_objects_count, state.scope, intent.object);
        const auto promise = resolve(state.promises, state.promise_count, state.scope, intent.promise);
        const auto account = account_index(state, intent.account);
        const ActorFact* latest = nullptr;
        if (promise) {
            for (std::size_t index = 0; index < state.view.account_count; ++index) {
                const auto& acquired = state.view.accounts[index];
                if (acquired.evidence == ActorEvidence::Observed && acquired.fact.promise == intent.promise) {
                    latest = &acquired.fact;
                }
            }
        }
        switch (intent.kind) {
        case IntentKind::OfferPromise: return person && object && intent.person != state.view.self && intent.due >= state.view.now;
        case IntentKind::DecidePromise:
            return latest && latest->kind == ActorFactKind::PromiseOffered && latest->other == state.view.self;
        case IntentKind::RequestRelease:
            return latest && (latest->kind == ActorFactKind::PromiseAccepted || latest->kind == ActorFactKind::ReleaseDeclined) &&
                latest->subject == state.view.self;
        case IntentKind::DecideRelease:
            return latest && latest->kind == ActorFactKind::ReleaseRequested && latest->other == state.view.self;
        case IntentKind::ConsentTransfer:
        case IntentKind::Transfer: return person && object;
        case IntentKind::Tell: return person && account;
        case IntentKind::Respond:
            if (!person || !account || intent.attitude_delta == 0 ||
                intent.attitude_delta < -5 || intent.attitude_delta > 5) { return false; }
            return state.view.accounts[*account].evidence == ActorEvidence::Observed &&
                (state.view.accounts[*account].fact.subject == intent.person ||
                 state.view.accounts[*account].fact.other == intent.person);
        }
        return false;
    }
    std::expected<Action, Error> bind(Family& family, const ActorState& state, const ActorIntent& intent) {
        if (!valid(state, intent)) { return std::unexpected(Error::Terms); }
        const auto person = resolve(state.people, state.view_people_count, state.scope, intent.person);
        const auto object = resolve(state.objects, state.view_objects_count, state.scope, intent.object);
        const auto promise = resolve(state.promises, state.promise_count, state.scope, intent.promise);
        const auto account = account_index(state, intent.account);
        switch (intent.kind) {
        case IntentKind::OfferPromise:
        case IntentKind::ConsentTransfer:
        case IntentKind::Transfer: {
            const auto current = family.object(object->raw);
            if (!current) { return std::unexpected(Error::Missing); }
            if (intent.kind == IntentKind::OfferPromise) {
                return Action{OfferPromise{person->raw, object->raw, intent.due, current->version}};
            }
            if (intent.kind == IntentKind::ConsentTransfer) {
                return Action{ConsentTransfer{object->raw, person->raw, current->version}};
            }
            return Action{Transfer{object->raw, person->raw, current->version}};
        }
        case IntentKind::DecidePromise:
        case IntentKind::RequestRelease:
        case IntentKind::DecideRelease: {
            const auto current = family.commitment(promise->raw);
            if (!current) { return std::unexpected(Error::Missing); }
            if (intent.kind == IntentKind::DecidePromise) {
                return Action{DecidePromise{promise->raw, current->version, intent.accept}};
            }
            if (intent.kind == IntentKind::RequestRelease) {
                return Action{RequestRelease{promise->raw, current->version}};
            }
            return Action{DecideRelease{promise->raw, current->version, intent.accept}};
        }
        case IntentKind::Tell: return Action{Tell{person->raw, state.accounts[*account]}};
        case IntentKind::Respond: return Action{Respond{person->raw, state.accounts[*account], intent.attitude_delta}};
        }
        return std::unexpected(Error::Unsupported);
    }
    Error deliver(Family& family, ActorState& state, ActorEntry& entry) {
        if (!entry.result) { return Error::Conflict; }
        if (!state.delivery_enabled) { return Error::Capacity; }
        const auto raw = *entry.result;
        ActorReceipt receipt{entry.key, ActorOutcome::Completed, {}};
        if (!raw) {
            const auto operation = entry.choice.intent.kind;
            const bool unavailable = raw.error == Error::Life || raw.error == Error::Consent ||
                raw.error == Error::NotHolder || raw.error == Error::NotContact;
            if ((operation != IntentKind::Transfer && operation != IntentKind::ConsentTransfer &&
                operation != IntentKind::Tell) || !unavailable) {
                return raw.error;
            }
            receipt.outcome = ActorOutcome::NotCompleted;
        } else {
            const auto error = project(family, state);
            if (error != Error::None) { return error; }
            if (entry.choice.intent.kind == IntentKind::OfferPromise) {
                for (std::size_t index = 0; index < state.promise_count; ++index) {
                    if (!state.promises[index].mention && state.promises[index].raw == raw.promise) {
                        receipt.promise = ActorAccess::ref<ActorPromiseTag>(state.scope, index + 1);
                        break;
                    }
                }
                if (!receipt.promise) { return Error::Conflict; }
            }
        }
        entry.receipt = receipt;
        return Error::None;
    }
}
