#pragma once

#include <cheryl/story/coordinator.h>

namespace CE::Story::Detail {
    struct ActorAccess {
        template<class Tag> static ActorRef<Tag> ref(std::uint64_t scope, std::size_t index) {
            ActorRef<Tag> result;
            result.scope_ = scope;
            result.local_ = static_cast<std::uint32_t>(index);
            return result;
        }
        template<class Tag> static std::optional<std::size_t> index(ActorRef<Tag> ref, std::uint64_t scope, std::size_t count) {
            if (ref.scope_ != scope || ref.local_ == 0 || ref.local_ > count) {
                return std::nullopt;
            }
            return ref.local_ - 1;
        }
        static std::shared_ptr<ActorState> lock(const ActorEndpoint& endpoint) { return endpoint.state_.lock(); }
        static ActorEndpoint endpoint(const std::shared_ptr<ActorState>& state) {
            ActorEndpoint result;
            result.state_ = state;
            return result;
        }
    };
    template<class Raw> struct Binding { Raw raw{}; bool actionable{}; bool mention{}; };
    struct ActorEntry {
        ActorAttemptKey key{};
        ActorChoice choice{};
        ActorReceipt receipt{};
        std::optional<Request> request;
        std::optional<Result> result;
        bool suspended{};
    };
    struct ActorState {
        const void* coordinator{};
        std::uint64_t scope{};
        PersonId person{};
        ActorView view{};
        std::array<Binding<PersonId>, 128> people{};
        std::array<Binding<ObjectId>, 128> objects{};
        std::size_t view_people_count{};
        std::size_t view_objects_count{};
        std::array<Binding<CommitmentId>, 128> promises{};
        std::size_t promise_count{};
        std::array<Binding<PartnershipId>, 128> partnerships{};
        std::size_t partnership_count{};
        std::array<std::uint64_t, 64> accounts{};
        std::array<ActorEntry, 32> entries{};
        std::size_t entry_count{};
        std::uint64_t retired{};
        bool delivery_enabled{true};
        bool available{true};
    };
    struct CoordinatorState {
        std::unique_ptr<Family> family;
        StoryTime now{};
        std::array<std::shared_ptr<ActorState>, 8> actors{};
        std::size_t actor_count{};
    };
    std::shared_ptr<ActorState> authenticate(CoordinatorState&, const ActorEndpoint&);
    Error project(Family&, ActorState&);
    bool valid(const ActorState&, const ActorIntent&);
    std::expected<Action, Error> bind(Family&, const ActorState&, const ActorIntent&);
    Error deliver(Family&, ActorState&, ActorEntry&);
}
