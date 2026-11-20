#include "state.h"
#include <atomic>
#include <new>

namespace CE::Story::Detail {
    std::shared_ptr<ActorState> authenticate(CoordinatorState& owner, const ActorEndpoint& endpoint) {
        auto actor = ActorAccess::lock(endpoint);
        return actor && actor->coordinator == &owner ? actor : nullptr;
    }
}

namespace CE::Story {
    namespace {
        std::unexpected<ServiceFault> fault(Error error, bool adopted = false) {
            return std::unexpected(ServiceFault{error, adopted});
        }
        bool room(const Detail::ActorState& actor) {
            return actor.view.account_count < actor.view.accounts.size() &&
                actor.view_people_count + 3 <= actor.people.size() &&
                actor.view_objects_count < actor.objects.size() &&
                actor.promise_count < actor.promises.size() &&
                actor.partnership_count < actor.partnerships.size();
        }
    }
    ActorCoordinator::ActorCoordinator(std::unique_ptr<Detail::CoordinatorState> state) : state_(std::move(state)) {}
    ActorCoordinator::~ActorCoordinator() = default;
    std::expected<std::unique_ptr<ActorCoordinator>, ServiceFault> ActorCoordinator::create(
        std::unique_ptr<Family> family, StoryTime now) {
        if (!family) { return fault(Error::Authority); }
        const auto order = family->order_at(now);
        if (!order) { return fault(order.error()); }
        try {
            auto state = std::make_unique<Detail::CoordinatorState>();
            state->family = std::move(family);
            state->now = now;
            return std::unique_ptr<ActorCoordinator>(new ActorCoordinator(std::move(state)));
        } catch (const std::bad_alloc&) { return fault(Error::Capacity); }
    }
    Family& ActorCoordinator::owner() { return *state_->family; }
    std::expected<ActorEndpoint, ServiceFault> ActorCoordinator::open(PersonId person) {
        const auto self = owner().person(person);
        if (!self) { return fault(Error::Authority); }
        if (state_->actor_count == state_->actors.size()) { return fault(Error::Capacity); }
        for (std::size_t index = 0; index < state_->actor_count; ++index) {
            if (state_->actors[index]->person == person) { return fault(Error::Duplicate); }
        }
        static std::atomic<std::uint64_t> next_scope{1};
        auto scope = next_scope.load();
        do {
            if (scope == std::numeric_limits<std::uint64_t>::max()) { return fault(Error::Capacity); }
        } while (!next_scope.compare_exchange_weak(scope, scope + 1));
        try {
            auto actor = std::make_shared<Detail::ActorState>();
            actor->coordinator = state_.get();
            actor->scope = scope;
            actor->person = person;
            actor->view.now = state_->now;
            actor->view.self = Detail::ActorAccess::ref<ActorPersonTag>(scope, 1);
            actor->people[0] = {person, true, false};
            actor->view_people_count = 1;
            actor->view.people[0] = {actor->view.self, self->name, true};
            const auto error = Detail::project(owner(), *actor);
            if (error != Error::None) { return fault(error); }
            state_->actors[state_->actor_count++] = actor;
            return Detail::ActorAccess::endpoint(actor);
        } catch (const std::bad_alloc&) { return fault(Error::Capacity); }
    }
    std::expected<KnownPersonRef, ServiceFault> ActorCoordinator::identify(
        const ActorEndpoint& endpoint, PersonId person, Text label) {
        const auto actor = Detail::authenticate(*state_, endpoint);
        if (!actor) { return fault(Error::Authority); }
        if (label.size > label.bytes.size()) { return fault(Error::Terms); }
        if (!owner().person(person)) { return fault(Error::Missing); }
        std::size_t index = 0;
        while (index < actor->view_people_count &&
            (actor->people[index].mention || actor->people[index].raw != person)) { ++index; }
        if (index == actor->people.size()) { return fault(Error::Capacity); }
        if (index == actor->view_people_count) { ++actor->view_people_count; }
        const auto reference = Detail::ActorAccess::ref<ActorPersonTag>(actor->scope, index + 1);
        actor->people[index] = {person, true, false};
        actor->view.people[index] = {reference, label, true};
        actor->view.person_count = actor->view_people_count;
        return reference;
    }
    std::expected<KnownObjectRef, ServiceFault> ActorCoordinator::identify(
        const ActorEndpoint& endpoint, ObjectId object, Text label) {
        const auto actor = Detail::authenticate(*state_, endpoint);
        if (!actor) { return fault(Error::Authority); }
        if (label.size > label.bytes.size()) { return fault(Error::Terms); }
        if (!owner().object(object)) { return fault(Error::Missing); }
        std::size_t index = 0;
        while (index < actor->view_objects_count &&
            (actor->objects[index].mention || actor->objects[index].raw != object)) { ++index; }
        if (index == actor->objects.size()) { return fault(Error::Capacity); }
        if (index == actor->view_objects_count) { ++actor->view_objects_count; }
        const auto reference = Detail::ActorAccess::ref<ActorObjectTag>(actor->scope, index + 1);
        actor->objects[index] = {object, true, false};
        actor->view.objects[index] = {reference, label, true};
        actor->view.object_count = actor->view_objects_count;
        return reference;
    }
    std::expected<void, ServiceFault> ActorCoordinator::advance(StoryTime now) {
        if (now < state_->now) { return fault(Error::Time); }
        const auto order = owner().order_at(now);
        if (!order) { return fault(order.error()); }
        state_->now = now;
        for (std::size_t index = 0; index < state_->actor_count; ++index) { state_->actors[index]->view.now = now; }
        return {};
    }
    std::expected<void, ServiceFault> ActorCoordinator::refresh(const ActorEndpoint& endpoint) {
        const auto actor = Detail::authenticate(*state_, endpoint);
        if (!actor) { return fault(Error::Authority); }
        const auto error = Detail::project(owner(), *actor);
        actor->available = error == Error::None;
        if (error != Error::None) { return fault(error); }
        return {};
    }
    std::expected<ActorOffer, ServiceFault> ActorCoordinator::offer(
        const ActorEndpoint& endpoint, const ActorIntent& intent, Text label) {
        const auto actor = Detail::authenticate(*state_, endpoint);
        if (!actor) { return fault(Error::Authority); }
        if (label.size > label.bytes.size() || !Detail::valid(*actor, intent)) { return fault(Error::Terms); }
        for (std::size_t index = 0; index < actor->view.offer_count; ++index) {
            const auto& offer = actor->view.offers[index];
            if (offer.intent == intent && offer.label == label) { return offer; }
        }
        if (actor->view.offer_count == actor->view.offers.size()) { return fault(Error::Capacity); }
        const auto reference = Detail::ActorAccess::ref<ActorOfferTag>(actor->scope, actor->view.offer_count + 1);
        const ActorOffer offer{reference, intent, label};
        actor->view.offers[actor->view.offer_count++] = offer;
        return offer;
    }
    std::expected<void, ServiceFault> ActorCoordinator::progress(const ActorEndpoint& endpoint) {
        const auto actor = Detail::authenticate(*state_, endpoint);
        if (!actor) { return fault(Error::Authority); }
        for (std::size_t index = 0; index < actor->entry_count; ++index) {
            auto& entry = actor->entries[index];
            if (entry.receipt.outcome != ActorOutcome::Pending) { continue; }
            if (!entry.result) {
                if (entry.suspended) { return fault(Error::Conflict); }
                for (std::size_t audience = 0; audience < state_->actor_count; ++audience) {
                    auto& recipient = *state_->actors[audience];
                    const auto error = Detail::project(owner(), recipient);
                    recipient.available = error == Error::None;
                    if (error != Error::None) { return fault(error); }
                    if (!room(recipient)) { return fault(Error::Capacity); }
                }
                const auto action = Detail::bind(owner(), *actor, entry.choice.intent);
                if (!action) { entry.suspended = true; return fault(action.error()); }
                const auto order = owner().order_at(state_->now);
                if (!order) { return fault(order.error()); }
                const auto capability = owner().actor(actor->person);
                const auto attempt = owner().allocate_attempt(capability);
                if (!attempt) { return fault(attempt.error()); }
                entry.request = Request{*attempt, *order, *action};
                entry.result = owner().apply(capability, *entry.request);
            }
            const auto error = Detail::deliver(owner(), *actor, entry);
            if (error != Error::None) { return fault(error, static_cast<bool>(*entry.result)); }
            return {};
        }
        return {};
    }
    std::expected<void, ServiceFault> ActorCoordinator::delivery(const ActorEndpoint& endpoint, bool enabled) {
        const auto actor = Detail::authenticate(*state_, endpoint);
        if (!actor) { return fault(Error::Authority); }
        actor->delivery_enabled = enabled;
        return {};
    }
}
