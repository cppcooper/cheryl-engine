#pragma once

#include <cheryl/export.h>
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

namespace CE::Events {
    /** Owned bus observations; cumulative counters and closure are not callback-drain barriers. */
    struct EventStats {
        ::CE::Diagnostics::DomainId domain = 0; ///< Process-local bus identity.
        std::uint64_t registrations = 0; ///< Total persistent listener registrations admitted.
        std::uint64_t active = 0; ///< Listeners still registered at the snapshot boundary.
        std::uint64_t dispatches = 0; ///< Admitted channel dispatch calls.
        std::uint64_t invocations = 0; ///< Listener invocation entries, not necessarily completed callbacks.
        std::uint64_t queued_failures = 0; ///< Deferred submission, callback or cancellation failures reported through error sinks.
        std::uint64_t discarded = 0; ///< Deliveries intentionally discarded after listener invalidation, including captured deferred work.
        bool closed = false; ///< Whether new registration/dispatch is rejected; running callbacks can remain.
    };
    /** An owned named-event registry. A bus separates registrations/lifecycle;
     * it does not select a thread. Immediate dispatch runs on its producer's thread.
     * String/any payload consistency remains the application author's contract.
     * Typed channels isolate name/type identity and check payloads before callback
     * entry, using the same persistent registration and optional delivery lifetime.
     */
    class EventBus final {
    public:
        /** Legacy named-event callback receiving an owned std::any payload copy; its type agreement belongs to callers. */
        using Callback = std::function<void(std::any)>;
        /** Move-only deferred task whose ownership transfers only when Delivery accepts it. */
        using Work = std::move_only_function<void()>;
        // Return true only after owning the deferred task. Reject by returning
        // false or throwing without retaining it. Never execute inline or wait.
        // Targets must preserve FIFO execution within a listener's stream.
        /** Deferred FIFO-per-listener transport: return true only after owning work; rejection returns false or throws without retaining it. Never invoke inline or block waiting. */
        using Delivery = std::function<bool(Work)>;
        // Required for queued delivery; must not throw and must own/protect its target
        // through pending-task destruction, independently of listener invalidation/waits.
        /** Required queued-error sink; must not throw and must protect captures through pending-task destruction independently of listener invalidation. */
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
            const ::CE::Diagnostics::DomainId domain = ::CE::Diagnostics::next_domain_id();
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

            CE_API explicit DeliveryTicket(std::shared_ptr<Listener> value);
            CE_API ~DeliveryTicket();
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
            ::CE::Diagnostics::DomainId bus_id_ = 0;

            Registration(const std::shared_ptr<State>& bus, const std::shared_ptr<Listener>& listener, std::uint64_t id)
            : bus_(bus), listener_(listener), id_(id), bus_id_(bus->counters->domain) {}

        public:
            Registration() = default;
            [[nodiscard]] std::uint64_t id() const { return id_; }
            [[nodiscard]] ::CE::Diagnostics::DomainId bus_id() const { return bus_id_; }
        };

        /** Copyable notification handle that does not retain the bus or Context.
         * Empty/expired handles discard observations without copying payloads.
         * Live handles use notify's failure isolation and producer-thread delivery.
         */
        class Publisher final {
            std::weak_ptr<State> state_;
            friend class EventBus;
            explicit Publisher(const std::shared_ptr<State>& state) noexcept : state_(state) {}

        public:
            Publisher() = default;

            /** True for an empty handle or one whose bus state has ended; closure may reject before expiration. */
            [[nodiscard]] bool expired() const noexcept { return state_.expired(); }

            /** Publish an exact-type observation; queued listeners retain their own error sinks.
             * Optional errors run synchronously and must not recursively notify this bus.
             * This handle never extends the lifetime of a callback's borrowed target.
             */
            template <::CE::Events::EventPayload Payload, typename Value>
                requires std::same_as<std::remove_cvref_t<Value>, Payload> && std::is_constructible_v<Payload, Value&&>
            void notify(
                const ::CE::Events::EventChannel<Payload>& event,
                Value&& payload,
                const ErrorHandler& errors = ErrorHandler{}
            ) const noexcept {
                if (const auto state = state_.lock()) {
                    try {
                        auto owned = std::make_any<Payload>(std::forward<Value>(payload));
                        notify_state(*state, event.name(), typeid(Payload), owned, errors);
                    } catch (...) {
                        report_notification_failure(errors, std::current_exception());
                    }
                }
            }
        };

        /** Borrow notification access without retaining this bus; obtain while the bus is alive. */
        [[nodiscard]] Publisher publisher() const noexcept { return Publisher{state_}; }

        EventBus() = default;
        CE_API ~EventBus();
        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        // A registration is intentionally persistent even when its ID is ignored.
        // Empty callback or queued delivery without errors throws invalid_args;
        // closed bus throws failed_operation. Strings/callables are owned after registration.
        CE_API Registration register_listener(
            const std::string& event,
            Callback callback,
            Delivery delivery = Delivery{},
            ErrorHandler errors = ErrorHandler{}
        );
        // Borrow payload for this call; listeners receive copies. Immediate exceptions
        // propagate, queued failures go to their error sink. A closed bus rejects dispatch.
        CE_API void dispatch(const std::string& event, const std::any& payload);
        // Exact name/type channel; callback copies/registration and delivery obey
        // the same lifetime as named listeners. Empty callbacks reject as usual.
        template <::CE::Events::EventPayload Payload>
        Registration register_listener(
            const ::CE::Events::EventChannel<Payload>& event,
            typename ::CE::Events::EventChannel<Payload>::Callback callback,
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
        template <::CE::Events::EventPayload Payload, typename Value>
            requires std::same_as<std::remove_cvref_t<Value>, Payload> && std::is_constructible_v<Payload, Value&&>
        void dispatch(const ::CE::Events::EventChannel<Payload>& event, Value&& payload) {
            auto owned = std::make_any<Payload>(std::forward<Value>(payload));
            dispatch_channel(event.name(), typeid(Payload), owned);
        }
        template <::CE::Events::EventPayload Payload, typename Value>
            requires std::same_as<std::remove_cvref_t<Value>, Payload> && std::is_constructible_v<Payload, Value&&>
        void notify(
            const ::CE::Events::EventChannel<Payload>& event,
            Value&& payload,
            const ErrorHandler& errors = ErrorHandler{}
        ) noexcept {
            try {
                auto owned = std::make_any<Payload>(std::forward<Value>(payload));
                notify_channel(event.name(), typeid(Payload), owned, errors);
            } catch (...) {
                report_notification_failure(errors, std::current_exception());
            }
        }
        // Invalidation prevents new invocation entry; already-running work finishes.
        CE_API bool unregister_listener(const Registration& registration);
        // Only wait after invalidation. Waiting on one's own invocation rejects.
        CE_API void wait_for_listener(const Registration& registration) const;
        CE_API bool unregister_and_wait(const Registration& registration);
        // Every concurrent close returns after all invocation gates are closed.
        // Already-running callbacks retain ownership and are not waited for.
        CE_API void close();
        [[nodiscard]] CE_API ::CE::Events::EventStats diagnostics() const;
        // Explicit unlocked observer; registry edits/native callbacks/destructors
        // do not emit ordinary logs. Payloads and names are not sampled.
        CE_API void report_diagnostics() const noexcept;

    private:
        CE_API Registration
        register_channel(const std::string& event, std::type_index payload, Callback callback, Delivery delivery, ErrorHandler errors);
        CE_API void dispatch_channel(std::string_view event, std::type_index type, const std::any& payload);
        CE_API void notify_channel(
            std::string_view event,
            std::type_index type,
            const std::any& payload,
            const ErrorHandler& errors
        ) noexcept;
        CE_API static void notify_state(
            State& state,
            std::string_view event,
            std::type_index type,
            const std::any& payload,
            const ErrorHandler& errors
        ) noexcept;
        CE_API static void report_notification_failure(const ErrorHandler& errors, std::exception_ptr failure) noexcept;
        CE_API static void invoke(const std::shared_ptr<Listener>& listener, const std::any& payload);
        CE_API static void invalidate(const std::shared_ptr<Listener>& listener);
        CE_API static void deliver(const std::shared_ptr<Listener>& listener, const std::any& payload);
        CE_API static void report_error(const std::shared_ptr<Listener>& listener, std::exception_ptr failure) noexcept;
    };
}

/**
 * @fn template<CE::Events::EventPayload Payload, typename Value> void CE::Events::EventBus::notify(const CE::Events::EventChannel<Payload>& event, Value&& payload, const ErrorHandler& errors) noexcept
 * @brief Attempt typed observation delivery without propagating notification failures.
 * @param event Exact name/type channel; legacy named listeners are separate.
 * @param payload Exact-type value copied/moved into owned delivery storage.
 * @param errors Optional synchronous sink for payload preparation, closed-bus, snapshot and immediate-listener failures.
 * The sink runs outside bus locks and must not recursively notify through this bus. An empty or throwing sink uses independent emergency reporting.
 * Each immediate listener is attempted despite earlier failures. Queued listeners retain their registration-owned error sinks and delivery lifetimes.
 * Invalidation still prevents invocation entry. Nested notification remains synchronous; source ordering/coalescing belongs to the producer.
 */

/**
 * @fn CE::Events::EventBus::EventBus()
 * @brief Create an open independently owned registry; immediate delivery is the default.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn CE::Events::EventBus::~EventBus()
 * @brief Close invocation gates and release registry ownership.
 * Does not wait for running callbacks; settle producers and retained callback dependencies before destroying them.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn CE::Events::EventBus::EventBus(const EventBus&)
 * @brief Copying a registry is disabled; compose independent owned buses explicitly.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn EventBus& CE::Events::EventBus::operator=(const EventBus&)
 * @brief Registry ownership cannot be copied through assignment.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn CE::Events::EventBus::Registration::Registration()
 * @brief Create an empty non-owning registration identifier; it owns no listener.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn std::uint64_t CE::Events::EventBus::Registration::id() const
 * @brief Read the bus-local listener ID; zero denotes the default empty identifier.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn CE::Diagnostics::DomainId CE::Events::EventBus::Registration::bus_id() const
 * @brief Read the originating bus diagnostic identity for correlation, not persistence.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn CE::Events::EventBus::Registration CE::Events::EventBus::register_listener(const std::string& event, Callback callback, Delivery delivery, ErrorHandler errors)
 * @brief Register a persistent listener on an exact legacy name.
 * @param event Name owned after registration; empty names are permitted.
 * @param callback Owned callable receiving a copied std::any; nonempty required.
 * @param delivery Optional owned deferred transport; absent invokes immediately on the producer thread.
 * @param errors Required nonthrowing error sink when delivery is supplied; owns/protects its captured dependencies.
 * @return Bus-qualified non-owning identifier; discarding it does not unsubscribe.
 * @throws CE::Exceptions::invalid_args For an empty callback or queued delivery without an error sink.
 * @throws CE::Exceptions::failed_operation For a closed bus or exhausted registration IDs.
 * Registry operations are synchronized; callbacks run outside registry locks and may reenter. Payload agreement remains application-owned.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn void CE::Events::EventBus::dispatch(const std::string& event, const std::any& payload)
 * @brief Dispatch a legacy named payload to the registration snapshot.
 * @param event Exact legacy channel name, separate from typed channels.
 * @param payload Borrowed for this call and copied for listeners/deferred ownership.
 * A closed bus rejects. Immediate callback exceptions propagate to the producer; deferred preparation/submission/callback failures and loss of still-active work reach the listener's error sink. Explicit unregister/close instead discards pending delivery without reporting cancellation. Concurrent invalidation prevents new callback entry even for an already captured registration snapshot.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn template<CE::Events::EventPayload Payload> CE::Events::EventBus::Registration CE::Events::EventBus::register_listener(const CE::Events::EventChannel<Payload>& event, typename CE::Events::EventChannel<Payload>::Callback callback, Delivery delivery, ErrorHandler errors)
 * @brief Register a typed listener on an exact name/type channel.
 * @tparam Payload Unqualified copy-constructible object type.
 * @param event Channel value; its name/type are retained, not the EventChannel object.
 * @param callback Nonempty owned callable borrowing an invocation-owned payload copy.
 * @param delivery Optional deferred FIFO transport with the same rules as named registration.
 * @param errors Nonthrowing sink required for deferred delivery.
 * @return Persistent non-owning registration ID.
 * Validation, closure, unsubscription and queued lifetime match the named overload. Different payload types or the legacy named channel remain isolated.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn template<CE::Events::EventPayload Payload, typename Value> void CE::Events::EventBus::dispatch(const CE::Events::EventChannel<Payload>& event, Value&& payload)
 * @brief Own an exact-type payload before dispatching its typed channel.
 * @tparam Payload Declared channel payload.
 * @tparam Value Must have exactly Payload as its unqualified type and construct Payload from the forwarded value.
 * @param event Exact name/type channel.
 * @param payload Value copied or moved into owned storage before callbacks.
 * Construction failures reach the producer. Immediate callback exceptions propagate; deferred failures use the listener error sink. No implicit conversion to another payload type/channel occurs.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn bool CE::Events::EventBus::unregister_listener(const Registration& registration)
 * @brief Invalidate a listener before removing it from the registry.
 * @param registration Non-owning identifier; foreign/expired/empty values do not remove another listener.
 * @return True only when the listener was removed from this bus.
 * Prevents new invocation entry; callbacks already running continue. This does not wait or require pending tasks to be destroyed.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn void CE::Events::EventBus::wait_for_listener(const Registration& registration) const
 * @brief Wait only for running invocations of an already invalidated listener.
 * @param registration Identifier originating from this bus.
 * @throws CE::Exceptions::invalid_args For a foreign/empty registration.
 * @throws CE::Exceptions::failed_operation If still active or waiting from that listener's own nested invocation.
 * An expired listener returns immediately. Pending deferred task/capture destruction is independent; do not block a thread needed by running callbacks.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn bool CE::Events::EventBus::unregister_and_wait(const Registration& registration)
 * @brief Invalidate/remove, then wait for running callback entries to finish.
 * @return Whether unregister_listener() removed the listener.
 * The same foreign/self-wait errors as wait_for_listener() apply. This is not a barrier for destruction of all queued transport captures.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn void CE::Events::EventBus::close()
 * @brief Idempotently close every invocation gate and reject new registration/dispatch.
 * All concurrent close calls return with gates invalidated. Running callbacks keep their invocation ownership and are not waited for; captured dependencies must remain protected.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn CE::Events::EventStats CE::Events::EventBus::diagnostics() const
 * @brief Copy synchronized registry counts and independently sampled cumulative counters.
 * @return Owned observations, not a callback-completion or transport-destruction barrier.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn void CE::Events::EventBus::report_diagnostics() const noexcept
 * @brief Emit an explicit unlocked registry summary without payload/name sampling.
 * Call outside registry edits, native callbacks and sensitive destruction paths. Reporting failures use the emergency path.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */
