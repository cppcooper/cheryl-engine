#include "view-state-internal.h"

#include <internals/exceptions.h>
#include <internals/failure-reporting.h>
#include <internals/compile-time-logging.hpp>

#include <condition_variable>
#include <limits>
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
    thread_local ViewBinding::Invocation* ViewBinding::invocation = nullptr;

    template <class Change> struct Binding final : ViewBinding {
        std::function<void(const Change&)> callback;

        explicit Binding(std::function<void(const Change&)> value)
        : callback(std::move(value)) {}

        void enter(const Change& change) noexcept {
            {
                std::lock_guard lock(mutex);
                const auto* authorized = static_cast<const Change*>(operation);
                if (owner != std::this_thread::get_id() || !active || entered || !authorized || change.seed != authorized->seed ||
                    change.current != authorized->current || change.previous != authorized->previous)
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

    template <class Snapshot, class Change> struct ViewSourceState {
        CE::Events::EventBus* bus;
        CE::Events::EventBus::Publisher publisher;
        mutable std::mutex mutex;
        Snapshot current;
        std::thread::id owner;
        std::shared_ptr<ViewBinding> head;
        std::shared_ptr<ViewBinding> tail;
        bool publishing = false;
        std::function<void()> before_notify;

        explicit ViewSourceState(CE::Events::EventBus& events)
        : bus(&events), publisher(events.publisher()) {}

        static const CE::Events::EventChannel<Change>& channel() {
            if constexpr (std::is_same_v<Snapshot, WindowState>)
                return window_state_changed_event;
            else
                return simulation_view_changed_event;
        }
        void require_owner() const {
            if (owner != std::this_thread::get_id() || current.closed)
                throw CE::Exceptions::failed_operation(CE_HERE, "View operation requires its live owner");
        }
        void prune() {
            // Only the owner changes links, and never during a detached fanout.
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
        void reserve(std::uint64_t source, std::uint64_t session) {
            std::lock_guard lock(mutex);
            current.source = source;
            current.session = session;
        }
        bool select(Snapshot next) {
            {
                std::lock_guard lock(mutex);
                if (owner == std::thread::id{})
                    owner = std::this_thread::get_id();
                require_owner();
                if (!current.source || !current.session)
                    throw CE::Exceptions::failed_operation(CE_HERE, "View source has no admitted session");
                if (publishing)
                    throw CE::Exceptions::failed_operation(CE_HERE, "View selection is not reentrant");
            }
            Change change;
            {
                std::lock_guard lock(mutex);
                if (current.initialized && current.logical_size == next.logical_size && current.framebuffer_size == next.framebuffer_size) {
                    if constexpr (std::is_same_v<Snapshot, SimulationView>)
                        return false;
                    else if (current.focused == next.focused)
                        return false;
                }
                if (current.revision == std::numeric_limits<std::uint64_t>::max())
                    throw CE::Exceptions::failed_operation(CE_HERE, "View revision exhausted");
                change.previous = current;
                next.source = current.source;
                next.session = current.session;
                next.revision = current.revision + 1;
                next.initialized = true;
                next.closed = false;
                current = next;
                change.current = current;
                publishing = true;
            }
            const auto begin = head;
            const auto end = tail;
            for (auto node = begin; node; node = node->next) {
                std::lock_guard lock(node->mutex);
                node->operation = &change;
                node->entered = false;
                node->failure = {};
                if (node == end)
                    break;
            }
            try {
                if (before_notify)
                    before_notify();
                publisher.notify(channel(), change);
            } catch (...) {
                CE::Diagnostics::report_failure("view observation", std::current_exception());
            }
            std::exception_ptr failure;
            std::uint64_t required_entries = 0;
            std::uint64_t fallbacks = 0;
            for (auto node = begin; node;) {
                auto next_node = node->next; // Pins a retired node through self-detach.
                {
                    std::lock_guard lock(node->mutex);
                    if (node->active && !node->entered)
                        ++fallbacks;
                }
                static_cast<Binding<Change>&>(*node).enter(change);
                {
                    std::lock_guard lock(node->mutex);
                    if (!failure)
                        failure = node->failure;
                    if (node->entered)
                        ++required_entries;
                    node->operation = nullptr;
                }
                if (node == end)
                    break;
                node = std::move(next_node);
            }
            {
                std::lock_guard lock(mutex);
                publishing = false;
            }
            prune();
            CE_LOG_DEBUG(
                CE::Logging::Categories::engine,
                "subsystem=events event={} operation=notification_attempt source={} session={} revision={} logical_width={} logical_height={} pixel_width={} pixel_height={} required_entries={} fallbacks={} required_failed={}",
                channel().name(), change.current.source, change.current.session, change.current.revision, change.current.logical_size.width,
                change.current.logical_size.height, change.current.framebuffer_size.width, change.current.framebuffer_size.height,
                required_entries, fallbacks, bool(failure)
            );
            if (failure)
                std::rethrow_exception(failure);
            return true;
        }
        void close() noexcept {
            std::shared_ptr<ViewBinding> retired;
            {
                std::lock_guard lock(mutex);
                current.closed = true;
                bus = nullptr;
                retired = std::move(head);
                tail.reset();
            }
            // No publication on destruction. Invalidate before releasing targets.
            for (auto node = retired; node; node = node->next)
                node->detach();
        }
    };

    std::uint64_t ViewSourceAccess::identity() {
        const auto value = CE::Diagnostics::next_domain_id();
        if (!value)
            throw CE::Exceptions::failed_operation(CE_HERE, "Event identity exhausted");
        return value;
    }
    WindowStateSource ViewSourceAccess::window_source(CE::Events::EventBus& bus) {
        return WindowStateSource(bus);
    }
    SimulationViewSource ViewSourceAccess::simulation_source(CE::Events::EventBus& bus) {
        return SimulationViewSource(bus);
    }
#define CE_VIEW_ACCESS(Snapshot, Change)                                                                                                   \
    void ViewSourceAccess::reserve(ViewSource<Snapshot, Change>& source, std::uint64_t id, std::uint64_t session) {                        \
        source.state_->reserve(id, session);                                                                                               \
    }                                                                                                                                      \
    bool ViewSourceAccess::select(ViewSource<Snapshot, Change>& source, Snapshot next) {                                                   \
        return source.state_->select(next);                                                                                                \
    }                                                                                                                                      \
    void ViewSourceAccess::close(ViewSource<Snapshot, Change>& source) noexcept {                                                          \
        source.state_->close();                                                                                                            \
    }                                                                                                                                      \
    void ViewSourceAccess::before_notify(ViewSource<Snapshot, Change>& source, std::function<void()> callback) {                           \
        source.state_->before_notify = std::move(callback);                                                                                \
    }
    CE_VIEW_ACCESS(WindowState, WindowStateChanged)
    CE_VIEW_ACCESS(SimulationView, SimulationViewChanged)
#undef CE_VIEW_ACCESS
}

namespace CE::Runtime {
    ViewAttachment::ViewAttachment(std::shared_ptr<Detail::ViewBinding> binding) noexcept
    : binding_(std::move(binding)) {}
    ViewAttachment::~ViewAttachment() {
        reset();
    }
    ViewAttachment::ViewAttachment(ViewAttachment&& other) noexcept
    : binding_(std::move(other.binding_)) {}
    ViewAttachment& ViewAttachment::operator=(ViewAttachment&& other) noexcept {
        if (this != &other) {
            reset();
            binding_ = std::move(other.binding_);
        }
        return *this;
    }
    void ViewAttachment::reset() noexcept {
        if (auto binding = std::exchange(binding_, {}))
            binding->detach();
    }
    bool ViewAttachment::attached() const noexcept {
        return binding_ && binding_->attached();
    }

    template <class Snapshot, class Change>
    ViewSource<Snapshot, Change>::ViewSource(CE::Events::EventBus& events)
    : state_(std::make_shared<Detail::ViewSourceState<Snapshot, Change>>(events)) {}

    template <class Snapshot, class Change> Snapshot ViewSource<Snapshot, Change>::snapshot() const {
        std::lock_guard lock(state_->mutex);
        return state_->current;
    }

    template <class Snapshot, class Change>
    ViewAttachment ViewSource<Snapshot, Change>::watch(std::function<void(const Change&)> callback) const {
        if (!callback)
            throw CE::Exceptions::invalid_args(CE_HERE, "View watch requires a callback");
        auto& state = *state_;
        Change seed;
        {
            std::lock_guard lock(state.mutex);
            state.require_owner();
            if (!state.current.initialized)
                throw CE::Exceptions::failed_operation(CE_HERE, "View watch requires an initialized source");
            seed = {state.current, state.current, true};
        }
        if (!state.publishing)
            state.prune();
        auto node = std::make_shared<Detail::Binding<Change>>(std::move(callback));
        node->registration = state.bus->register_listener(state.channel(), [weak = std::weak_ptr(node)](const Change& change) {
            if (auto retained = weak.lock())
                retained->enter(change);
        });
        ViewAttachment attachment(node);
        // A private seed belongs only to this attachment, including a watch added
        // from another watch's callback. It never re-enters global fanout.
        {
            std::lock_guard lock(node->mutex);
            node->operation = &seed;
        }
        node->enter(seed);
        {
            std::lock_guard lock(node->mutex);
            node->operation = nullptr;
        }
        if (node->failure)
            std::rethrow_exception(node->failure);
        if (state.tail)
            state.tail->next = node;
        else
            state.head = node;
        state.tail = node;
        return attachment;
    }
    template class ViewSource<WindowState, WindowStateChanged>;
    template class ViewSource<SimulationView, SimulationViewChanged>;
}
