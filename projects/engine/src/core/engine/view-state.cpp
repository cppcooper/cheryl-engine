#include "view-state-internal.h"
#include "required-bindings.h"

#include <internals/exceptions.h>
#include <internals/failure-reporting.h>
#include <internals/compile-time-logging.hpp>

#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace CE::Runtime::Detail {
    thread_local ViewBinding::Invocation* ViewBinding::invocation = nullptr;

    template <class Snapshot, class Change> struct ViewSourceState {
        CE::Events::EventBus* bus;
        CE::Events::EventBus::Publisher publisher;
        mutable std::mutex mutex;
        Snapshot current;
        std::thread::id owner;
        RequiredBindings<Change> bindings;
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
            const auto result = bindings.publish(change, publisher, channel(), before_notify);
            {
                std::lock_guard lock(mutex);
                publishing = false;
            }
            CE_LOG_DEBUG(
                CE::Logging::Categories::engine,
                "subsystem=events event={} operation=notification_attempt source={} session={} revision={} logical_width={} logical_height={} pixel_width={} pixel_height={} required_entries={} fallbacks={} required_failed={}",
                channel().name(), change.current.source, change.current.session, change.current.revision, change.current.logical_size.width,
                change.current.logical_size.height, change.current.framebuffer_size.width, change.current.framebuffer_size.height,
                result.entered, result.fallbacks, bool(result.failure)
            );
            if (result.failure)
                std::rethrow_exception(result.failure);
            return true;
        }
        void close() noexcept {
            {
                std::lock_guard lock(mutex);
                current.closed = true;
                bus = nullptr;
            }
            bindings.close();
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
        return ViewAttachment(state.bindings.attach(*state.bus, state.channel(), std::move(callback), &seed));
    }
    template class ViewSource<WindowState, WindowStateChanged>;
    template class ViewSource<SimulationView, SimulationViewChanged>;
}
