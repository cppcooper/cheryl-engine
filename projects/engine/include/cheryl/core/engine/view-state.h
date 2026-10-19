#pragma once

#include <cheryl/export.h>
#include <core/display/window-interface.h>
#include <core/subsystems/event-bus.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace CE::Runtime {
    class Context;
    namespace Detail {
        struct ViewSourceAccess;
        struct ViewBinding;
        template <class Snapshot, class Change> struct ViewSourceState;
    }

    /** An owned selection made on the platform owner; focus is unknown when unsupported. */
    struct WindowState {
        std::uint64_t source = 0;
        std::uint64_t session = 0;
        std::uint64_t revision = 0;
        CE::Platform::Dimensions<int> logical_size{0, 0};
        CE::Platform::FramebufferSize framebuffer_size{0, 0};
        std::optional<bool> focused;
        std::chrono::steady_clock::time_point observed_at{};
        bool initialized = false;
        bool closed = false;
        bool operator==(const WindowState&) const = default;
    };

    /** Dimensions selected by the simulation owner before an actual update. */
    struct SimulationView {
        std::uint64_t source = 0;
        std::uint64_t session = 0;
        std::uint64_t revision = 0;
        std::uint64_t window_revision = 0;
        CE::Platform::Dimensions<int> logical_size{0, 0};
        CE::Platform::FramebufferSize framebuffer_size{0, 0};
        std::chrono::steady_clock::time_point observed_at{};
        bool initialized = false;
        bool closed = false;
        bool operator==(const SimulationView&) const = default;
    };

    struct WindowStateChanged {
        WindowState previous;
        WindowState current;
        bool seed = false; ///< Watch initialization, rather than a source transition.
    };
    struct SimulationViewChanged {
        SimulationView previous;
        SimulationView current;
        bool seed = false;
    };
    inline const CE::Events::EventChannel<WindowStateChanged> window_state_changed_event{"window-state-changed"};
    inline const CE::Events::EventChannel<SimulationViewChanged> simulation_view_changed_event{"simulation-view-changed"};

    /** Owns an immediate required watch. Reset acknowledges invalidation and waits
     * for an entered callback, except when called by that callback itself.
     * Callback captures survive entry; no callback is made during destruction.
     */
    class ViewAttachment final {
        std::shared_ptr<Detail::ViewBinding> binding_;
        template <class Snapshot, class Change> friend class ViewSource;
        explicit ViewAttachment(std::shared_ptr<Detail::ViewBinding> binding) noexcept;

    public:
        ViewAttachment() = default;
        CE_API ~ViewAttachment();
        CE_API ViewAttachment(ViewAttachment&&) noexcept;
        CE_API ViewAttachment& operator=(ViewAttachment&&) noexcept;
        ViewAttachment(const ViewAttachment&) = delete;
        ViewAttachment& operator=(const ViewAttachment&) = delete;
        CE_API void reset() noexcept;
        [[nodiscard]] CE_API bool attached() const noexcept;
    };

    /** Retained read/watch access to one Context-owned view source. Snapshot is
     * synchronized. Watch requires the initialized source's owner, applies one
     * protected seed before returning, and propagates sizing failures.
     * Ordinary observations use the corresponding typed bus channel instead.
     */
    template <class Snapshot, class Change> class ViewSource final {
        std::shared_ptr<Detail::ViewSourceState<Snapshot, Change>> state_;
        friend class Context;
        friend struct Detail::ViewSourceAccess;
        CE_API explicit ViewSource(CE::Events::EventBus& events);

    public:
        [[nodiscard]] CE_API Snapshot snapshot() const;
        [[nodiscard]] CE_API ViewAttachment watch(std::function<void(const Change&)> callback) const;
    };
    using WindowStateSource = ViewSource<WindowState, WindowStateChanged>;
    using SimulationViewSource = ViewSource<SimulationView, SimulationViewChanged>;
    extern template class CE_API ViewSource<WindowState, WindowStateChanged>;
    extern template class CE_API ViewSource<SimulationView, SimulationViewChanged>;
}
