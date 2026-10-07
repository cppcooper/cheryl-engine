#pragma once
#include <core/diagnostics.h>
#include "event-channel.h"

#include <any>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace CE::SubSystems {
    struct EventStats {
        Diagnostics::DomainId domain = 0;
        std::uint64_t registrations = 0;
        std::uint64_t active = 0;
        std::uint64_t dispatches = 0;
        std::uint64_t invocations = 0;
        std::uint64_t queued_failures = 0;
        std::uint64_t discarded = 0;
        bool closed = false;
    };
    /** An owned named-event registry. A bus separates registrations/lifecycle;
     * it does not select a thread. Immediate dispatch runs on its producer's thread.
     * String/any payload consistency remains the application author's contract.
     * Typed channels isolate name/type identity and check payloads before callback
     * entry, using the same persistent registration and optional delivery lifetime.
     */
    class EventBus final {
    public:
        using Callback = std::function<void(std::any)>;
        using Work = std::move_only_function<void()>;
        // Return true only after owning the deferred task. Reject by returning
        // false or throwing without retaining it. Never execute inline or wait.
        // Targets must preserve FIFO execution within a listener's stream.
        using Delivery = std::function<bool(Work)>;
        // Required for queued delivery; must not throw and must own/protect its target
        // through pending-task destruction, independently of listener invalidation/waits.
        using ErrorHandler = std::function<void(std::exception_ptr)>;

    private:
        struct ChannelKey {
            std::string name;
            std::type_index payload = typeid(void); // Reserved for legacy named channels.
        };
        struct ChannelView {
            std::string_view name;
            std::type_index payload;
        };
        struct ChannelHash {
            using is_transparent = void;
            std::size_t operator()(ChannelView channel) const noexcept {
                return std::hash<std::string_view>{}(channel.name) ^ channel.payload.hash_code();
            }
            std::size_t operator()(const ChannelKey& channel) const noexcept {
                return (*this)(ChannelView{channel.name, channel.payload});
            }
        };
        struct ChannelEqual {
            using is_transparent = void;
            bool operator()(const ChannelKey& left, ChannelView right) const noexcept {
                return left.name == right.name && left.payload == right.payload;
            }
            bool operator()(ChannelView left, const ChannelKey& right) const noexcept { return (*this)(right, left); }
            bool operator()(const ChannelKey& left, const ChannelKey& right) const noexcept {
                return (*this)(left, ChannelView{right.name, right.payload});
            }
        };
        struct Counters {
            const Diagnostics::DomainId domain = Diagnostics::next_domain_id();
            std::atomic<std::uint64_t> invocations{0};
            std::atomic<std::uint64_t> queued_failures{0};
            std::atomic<std::uint64_t> discarded{0};
        };
        struct Listener {
            std::shared_ptr<Counters> counters;
            ChannelKey event;
            Callback callback;
            Delivery delivery;
            ErrorHandler errors;
            std::exception_ptr cancellation;
            std::mutex posting;
            std::mutex mutex;
            std::condition_variable idle;
            std::size_t running = 0;
            bool active = true;
        };
        struct DeliveryTicket {
            std::shared_ptr<Listener> listener;
            std::any payload;
            std::exception_ptr failure;
            std::atomic<bool> entered{false};

            explicit DeliveryTicket(std::shared_ptr<Listener> value);
            ~DeliveryTicket();
        };
        struct State {
            std::mutex mutex;
            std::unordered_map<ChannelKey, std::vector<std::shared_ptr<Listener>>, ChannelHash, ChannelEqual> channels;
            std::uint64_t next_id = 1;
            bool closed = false;
            std::shared_ptr<Counters> counters = std::make_shared<Counters>();
            std::uint64_t registrations = 0;
            std::uint64_t active = 0;
            std::uint64_t dispatches = 0;
        };
        std::shared_ptr<State> state_ = std::make_shared<State>();

    public:
        /** A copyable, bus-qualified identifier, not a subscription owner.
         * Discarding this value does not remove the persistent listener.
         */
        class Registration final {
            friend class EventBus;
            std::weak_ptr<State> bus_;
            std::weak_ptr<Listener> listener_;
            std::uint64_t id_ = 0;
            Diagnostics::DomainId bus_id_ = 0;

            Registration(const std::shared_ptr<State>& bus, const std::shared_ptr<Listener>& listener, std::uint64_t id)
            : bus_(bus), listener_(listener), id_(id), bus_id_(bus->counters->domain) {}

        public:
            Registration() = default;
            [[nodiscard]] std::uint64_t id() const { return id_; }
            [[nodiscard]] Diagnostics::DomainId bus_id() const { return bus_id_; }
        };

        EventBus() = default;
        ~EventBus();
        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        // A registration is intentionally persistent even when its ID is ignored.
        // Empty callback or queued delivery without errors throws invalid_args;
        // closed bus throws failed_operation. Strings/callables are owned after registration.
        Registration register_listener(
            const std::string& event,
            Callback callback,
            Delivery delivery = Delivery{},
            ErrorHandler errors = ErrorHandler{}
        );
        // Borrow payload for this call; listeners receive copies. Immediate exceptions
        // propagate, queued failures go to their error sink. A closed bus rejects dispatch.
        void dispatch(const std::string& event, const std::any& payload);
        // Exact name/type channel; callback copies/registration and delivery obey
        // the same lifetime as named listeners. Empty callbacks reject as usual.
        template <EventPayload Payload>
        Registration register_listener(
            const EventChannel<Payload>& event,
            typename EventChannel<Payload>::Callback callback,
            Delivery delivery = Delivery{},
            ErrorHandler errors = ErrorHandler{}
        ) {
            Callback erased;
            if (callback) {
                erased = [callback = std::move(callback)](std::any payload) {
                    const auto* value = std::any_cast<Payload>(&payload);
                    if (!value)
                        throw std::bad_any_cast{};
                    callback(*value);
                };
            }
            return register_channel(event.name(), typeid(Payload), std::move(erased), std::move(delivery), std::move(errors));
        }
        // Own an exact-type payload before dispatch. Construction failure reaches
        // the producer; per-listener queued failures reach their existing error sink.
        template <EventPayload Payload, typename Value>
            requires std::same_as<std::remove_cvref_t<Value>, Payload> && std::is_constructible_v<Payload, Value&&>
        void dispatch(const EventChannel<Payload>& event, Value&& payload) {
            auto owned = std::make_any<Payload>(std::forward<Value>(payload));
            dispatch_channel(event.name(), typeid(Payload), owned);
        }
        // Invalidation prevents new invocation entry; already-running work finishes.
        bool unregister_listener(const Registration& registration);
        // Only wait after invalidation. Waiting on one's own invocation rejects.
        void wait_for_listener(const Registration& registration) const;
        bool unregister_and_wait(const Registration& registration);
        // Every concurrent close returns after all invocation gates are closed.
        // Already-running callbacks retain ownership and are not waited for.
        void close();
        [[nodiscard]] EventStats diagnostics() const;
        // Explicit unlocked observer; registry edits/native callbacks/destructors
        // do not emit ordinary logs. Payloads and names are not sampled.
        void report_diagnostics() const noexcept;

    private:
        Registration register_channel(
            const std::string& event,
            std::type_index payload,
            Callback callback,
            Delivery delivery,
            ErrorHandler errors
        );
        void dispatch_channel(std::string_view event, std::type_index type, const std::any& payload);
        static void invoke(const std::shared_ptr<Listener>& listener, const std::any& payload);
        static void invalidate(const std::shared_ptr<Listener>& listener);
        static void deliver(const std::shared_ptr<Listener>& listener, const std::any& payload);
        static void report_error(const std::shared_ptr<Listener>& listener, std::exception_ptr failure) noexcept;
    };
}
