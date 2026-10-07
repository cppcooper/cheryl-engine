#pragma once
#include <core/diagnostics.h>

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
     * TODO: Add typed channels separately from delivery and registration lifetime.
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
        struct Counters {
            const Diagnostics::DomainId domain = Diagnostics::next_domain_id();
            std::atomic<std::uint64_t> invocations{0};
            std::atomic<std::uint64_t> queued_failures{0};
            std::atomic<std::uint64_t> discarded{0};
        };
        struct Listener {
            std::shared_ptr<Counters> counters;
            std::string event;
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
            std::unordered_map<std::string, std::vector<std::shared_ptr<Listener>>> channels;
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
        static void invoke(const std::shared_ptr<Listener>& listener, const std::any& payload);
        static void invalidate(const std::shared_ptr<Listener>& listener);
        static void deliver(const std::shared_ptr<Listener>& listener, const std::any& payload);
        static void report_error(const std::shared_ptr<Listener>& listener, std::exception_ptr failure) noexcept;
    };
}
