#pragma once

#include <any>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace CE::SubSystems {
    /** An owned named-event registry. A bus separates registrations/lifecycle;
     * it does not select a thread. Immediate dispatch runs on its producer's thread.
     * String/any payload consistency remains the application author's contract.
     * TODO: Add typed channels separately from delivery and registration lifetime.
     */
    class EventBus final {
    public:
        using Callback = std::function<void(std::any)>;

    private:
        struct Listener {
            std::string event;
            Callback callback;
            std::mutex mutex;
            std::condition_variable idle;
            std::size_t running = 0;
            bool active = true;
        };
        struct State {
            std::mutex mutex;
            std::unordered_map<std::string, std::vector<std::shared_ptr<Listener>>> channels;
            std::uint64_t next_id = 1;
            bool closed = false;
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

            Registration(const std::shared_ptr<State>& bus, const std::shared_ptr<Listener>& listener, std::uint64_t id)
            : bus_(bus), listener_(listener), id_(id) {}

        public:
            Registration() = default;
            [[nodiscard]] std::uint64_t id() const { return id_; }
        };

        EventBus() = default;
        ~EventBus();
        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        // A registration is intentionally persistent even when its ID is ignored.
        Registration register_listener(const std::string& event, Callback callback);
        void dispatch(const std::string& event, const std::any& payload);
        // Invalidation prevents new invocation entry; already-running work finishes.
        bool unregister_listener(const Registration& registration);
        // Only wait after invalidation. Waiting on one's own invocation rejects.
        void wait_for_listener(const Registration& registration) const;
        bool unregister_and_wait(const Registration& registration);
        void close();

    private:
        static void invoke(const std::shared_ptr<Listener>& listener, const std::any& payload);
        static void invalidate(const std::shared_ptr<Listener>& listener);
    };
}
