#pragma once

#include "actor.h"
#include "actions.h"

namespace CE::Story {
    namespace Detail { struct CoordinatorState; }
    struct ServiceFault {
        Error error{Error::None};
        bool after_adoption{};
    };
    class ActorCoordinator {
        std::unique_ptr<Detail::CoordinatorState> state_;
        explicit ActorCoordinator(std::unique_ptr<Detail::CoordinatorState>);
    public:
        ~ActorCoordinator();
        ActorCoordinator(const ActorCoordinator&) = delete;
        ActorCoordinator& operator=(const ActorCoordinator&) = delete;
        static std::expected<std::unique_ptr<ActorCoordinator>, ServiceFault> create(std::unique_ptr<Family>, StoryTime);
        Family& owner();
        std::expected<ActorEndpoint, ServiceFault> open(PersonId);
        std::expected<KnownPersonRef, ServiceFault> identify(const ActorEndpoint&, PersonId, Text);
        std::expected<KnownObjectRef, ServiceFault> identify(const ActorEndpoint&, ObjectId, Text);
        std::expected<void, ServiceFault> advance(StoryTime);
        std::expected<void, ServiceFault> refresh(const ActorEndpoint&);
        std::expected<ActorOffer, ServiceFault> offer(const ActorEndpoint&, const ActorIntent&, Text);
        std::expected<void, ServiceFault> progress(const ActorEndpoint&);
        // A trusted delivery gate can defer delivery without discarding an adopted result.
        std::expected<void, ServiceFault> delivery(const ActorEndpoint&, bool enabled);
    };
}
