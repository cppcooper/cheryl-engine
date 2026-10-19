#pragma once

#include <core/subsystems/event-bus.h>
#include <internals/exceptions.h>
#include <internals/failure-reporting.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

namespace CE::Runtime::Detail {
    struct ViewBinding {
        std::mutex mutex;
        std::condition_variable idle;
        bool active = true;
        bool entered = false;
        const std::thread::id owner = std::this_thread::get_id();
        std::size_t running = 0;
        const void* operation = nullptr;
        std::exception_ptr failure;
        CE::Events::EventBus::Registration registration;
        std::shared_ptr<ViewBinding> next;
        virtual ~ViewBinding() = default;

        struct Invocation {
            ViewBinding* binding;
            Invocation* previous;
        };
        static thread_local Invocation* invocation;

        void detach() noexcept {
            {
                std::lock_guard lock(mutex);
                active = false;
            }
            registration.disconnect();
            for (auto* current = invocation; current; current = current->previous)
                if (current->binding == this)
                    return;
            std::unique_lock lock(mutex);
            idle.wait(lock, [this] { return running == 0; });
        }
        bool attached() {
            std::lock_guard lock(mutex);
            return active;
        }
    };
    template <class Change> struct Binding final : ViewBinding {
        std::function<void(const Change&)> callback;

        explicit Binding(std::function<void(const Change&)> value)
        : callback(std::move(value)) {}

        void enter(const Change& change) noexcept {
            {
                std::lock_guard lock(mutex);
                const auto* authorized = static_cast<const Change*>(operation);
                const auto matches = [&] {
                    if constexpr (requires {
                                      change.seed;
                                      change.current;
                                      change.previous;
                                  })
                        return change.seed == authorized->seed && change.current == authorized->current &&
                               change.previous == authorized->previous;
                    else
                        return change == *authorized;
                };
                if (owner != std::this_thread::get_id() || !active || entered || !authorized || !matches())
                    return;
                entered = true; // Before effects: partial failure is never retried.
                ++running;
            }
            Invocation current{this, invocation};
            invocation = &current;
            std::exception_ptr error;
            try {
                callback(change);
            } catch (...) {
                error = std::current_exception();
            }
            invocation = current.previous;
            {
                std::lock_guard lock(mutex);
                failure = error;
                --running;
                idle.notify_all();
            }
        }
    };

    // Shared entry/lifetime machinery only: source state and phase semantics stay
    // with their concrete owners. Enumeration allocates only during attachment.
    template <class Payload> struct RequiredBindings {
        struct Result {
            std::exception_ptr failure;
            std::uint64_t entered = 0;
            std::uint64_t fallbacks = 0;
        };
        std::shared_ptr<ViewBinding> head;
        std::shared_ptr<ViewBinding> tail;
        bool publishing = false;

        void prune() {
            auto* link = &head;
            std::shared_ptr<ViewBinding> retired;
            tail.reset();
            while (*link) {
                const auto node = *link;
                if (!node->attached()) {
                    *link = node->next;
                    node->next = std::move(retired);
                    retired = node;
                } else {
                    tail = node;
                    link = &node->next;
                }
            }
        }
        std::shared_ptr<ViewBinding> attach(
            CE::Events::EventBus& bus,
            const CE::Events::EventChannel<Payload>& channel,
            std::function<void(const Payload&)> callback,
            const Payload* seed = nullptr
        ) {
            if (!callback)
                throw CE::Exceptions::invalid_args(CE_HERE, "Required attachment needs a callback");
            if (!publishing)
                prune();
            auto node = std::make_shared<Binding<Payload>>(std::move(callback));
            node->registration = bus.register_listener(channel, [weak = std::weak_ptr(node)](const Payload& payload) {
                if (const auto retained = weak.lock())
                    retained->enter(payload);
            });
            if (seed) {
                {
                    std::lock_guard lock(node->mutex);
                    node->operation = seed;
                }
                node->enter(*seed);
                {
                    std::lock_guard lock(node->mutex);
                    node->operation = nullptr;
                }
                if (node->failure) {
                    node->detach();
                    std::rethrow_exception(node->failure);
                }
            }
            if (tail)
                tail->next = node;
            else
                head = node;
            tail = node;
            return node;
        }
        Result publish(
            const Payload& payload,
            const CE::Events::EventBus::Publisher& publisher,
            const CE::Events::EventChannel<Payload>& channel,
            const std::function<void()>& before_notify = {}
        ) {
            if (publishing)
                throw CE::Exceptions::failed_operation(CE_HERE, "Required publication is not reentrant");
            publishing = true;
            const auto begin = head;
            const auto end = tail;
            for (auto node = begin; node; node = node->next) {
                std::lock_guard lock(node->mutex);
                node->operation = &payload;
                node->entered = false;
                node->failure = {};
                if (node == end)
                    break;
            }
            try {
                if (before_notify)
                    before_notify();
                publisher.notify(channel, payload);
            } catch (...) {
                CE::Diagnostics::report_failure("required notification", std::current_exception());
            }
            Result result;
            for (auto node = begin; node;) {
                auto next = node->next;
                {
                    std::lock_guard lock(node->mutex);
                    if (node->active && !node->entered)
                        ++result.fallbacks;
                }
                static_cast<Binding<Payload>&>(*node).enter(payload);
                {
                    std::lock_guard lock(node->mutex);
                    if (!result.failure)
                        result.failure = node->failure;
                    if (node->entered)
                        ++result.entered;
                    node->operation = nullptr;
                }
                if (node == end)
                    break;
                node = std::move(next);
            }
            publishing = false;
            prune();
            return result;
        }
        void close() noexcept {
            auto retired = std::move(head);
            tail.reset();
            for (auto node = retired; node; node = node->next)
                node->detach();
        }
    };
}
