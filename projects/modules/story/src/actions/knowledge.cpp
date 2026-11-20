#include "prepared.h"

namespace CE::Story::Detail {
    Error prepare(Prepared& change, const Perceive& action) noexcept {
        auto& store = change.store;
        if (!store.scoped(action.incident)) {
            return Error::ForeignReference;
        }
        const auto* episode = store.episodes.find(action.incident);
        count(store.counters.records_read);
        if (!episode || (episode->audience[0] != change.actor && episode->audience[1] != change.actor)) {
            return Error::Ineligible;
        }
        const auto* person = store.people.find(change.actor);
        for (auto index = person->accounts; index != absent; index = store.accounts.rows[index].next) {
            if (!change.step()) {
                return Error::WorkLimit;
            }
            count(store.counters.links_traversed);
            if (store.accounts.rows[index].source == action.incident) {
                return Error::Terminal;
            }
        }
        change.episode.fact = {EpisodeKind::Perceived, change.actor, {}, {}, {}};
        change.episode.support = episode->id;
        return change.acquire(change.actor, episode->fact, Evidence::Observed, {}, episode->id);
    }
    Error prepare(Prepared& change, const Tell& action) noexcept {
        auto& store = change.store;
        if (!store.scoped(action.recipient)) {
            return Error::ForeignReference;
        }
        if (action.recipient == change.actor || !store.alive(action.recipient, change.order.time)) {
            return Error::Life;
        }
        const auto* account = store.accounts.find({change.actor, action.account});
        count(store.counters.records_read);
        if (!account) {
            return Error::UnknownAccount;
        }
        const auto* actor = store.people.find(change.actor);
        bool contact = false;
        for (auto index = actor->contacts; index != absent; index = store.links[index].next) {
            if (!change.step()) {
                return Error::WorkLimit;
            }
            count(store.counters.links_traversed);
            if (store.links[index].other == action.recipient) {
                contact = true;
                break;
            }
        }
        if (!contact) {
            return Error::NotContact;
        }
        change.episode.fact = {EpisodeKind::Told, change.actor, action.recipient, {}, {}};
        change.episode.support = account->source;
        change.episode.audience[1] = action.recipient;
        return change.acquire(action.recipient, account->value.proposition, Evidence::Attributed, change.actor, change.episode.id);
    }
    Error prepare(Prepared& change, const Respond& action) noexcept {
        auto& store = change.store;
        if (!store.scoped(action.contact)) {
            return Error::ForeignReference;
        }
        const auto* account = store.accounts.find({change.actor, action.account});
        count(store.counters.records_read);
        if (!account) {
            return Error::UnknownAccount;
        }
        if ((account->value.proposition.subject != action.contact && account->value.proposition.other != action.contact) ||
            action.attitude_delta < -5 || action.attitude_delta > 5 || action.attitude_delta == 0) {
            return Error::Terms;
        }
        if (!store.alive(action.contact, change.order.time)) {
            return Error::Life;
        }
        const auto* actor = store.people.find(change.actor);
        for (auto index = actor->contacts; index != absent; index = store.links[index].next) {
            if (!change.step()) {
                return Error::WorkLimit;
            }
            count(store.counters.links_traversed);
            auto& contact = store.links[index];
            if (contact.other != action.contact) {
                continue;
            }
            auto value = contact;
            value.attitude = static_cast<std::int16_t>(std::clamp<int>(value.attitude + action.attitude_delta, -100, 100));
            change.contact = Replacement<ContactLink>{&contact, value};
            change.episode.fact = {EpisodeKind::Response, change.actor, action.contact, {}, {}};
            change.episode.support = account->source;
            return Error::None;
        }
        return Error::NotContact;
    }
}
