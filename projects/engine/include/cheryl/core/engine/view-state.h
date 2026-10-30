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
    class RuntimeServices;
    namespace Detail {
        struct ViewSourceAccess;
        struct ViewBinding;
        template <class Snapshot, class Change> struct ViewSourceState;
    }

    /** An owned selection made on the platform owner; focus is unknown when unsupported.
     * @see [Committed views](cheryl-guide:runtime/display-and-window-contract#committed-window-and-simulation-views)
     */
    struct WindowState {
        std::uint64_t source = 0; ///< Checked source identity after session reservation; zero before reservation.
        std::uint64_t session = 0; ///< Checked runtime session identity; not a native window ID.
        std::uint64_t revision = 0; ///< Advances on the initial selection and logical/pixel/supported-focus changes.
        CE::Platform::Dimensions<int> logical_size{0, 0}; ///< Copied window coordinates, independent of framebuffer pixels.
        CE::Platform::FramebufferSize framebuffer_size{0, 0}; ///< Copied drawable pixels; zero extents can suppress drawing.
        std::optional<bool> focused; ///< Native window focus when supported; unrelated to keyboard routing ownership.
        std::chrono::steady_clock::time_point observed_at{}; ///< Time of this committed selection.
        bool initialized = false; ///< A platform baseline has been selected.
        bool closed = false; ///< Runtime closed the source; closure does not fabricate a native change.
        bool operator==(const WindowState&) const = default;
    };

    /** Dimensions selected by the simulation owner before an actual update. */
    struct SimulationView {
        std::uint64_t source = 0; ///< Checked simulation-view source identity, separate from the window source.
        std::uint64_t session = 0; ///< Runtime session owning this selection.
        std::uint64_t revision = 0; ///< Advances for the baseline and changed size pairs; not for focus-only changes.
        std::uint64_t window_revision = 0; ///< Originating platform revision when this size selection committed.
        CE::Platform::Dimensions<int> logical_size{0, 0}; ///< Logical dimensions used by the matching game tick.
        CE::Platform::FramebufferSize framebuffer_size{0, 0}; ///< Pixel dimensions from the same copied handoff.
        std::chrono::steady_clock::time_point observed_at{}; ///< Time this simulation selection committed.
        bool initialized = false; ///< Simulation has selected its baseline.
        bool closed = false; ///< Runtime closed this source after owner settlement.
        bool operator==(const SimulationView&) const = default;
    };

    /** Owned platform selection fact; ordinary bus subscriptions have no seed/replay. */
    struct WindowStateChanged {
        WindowState previous; ///< Selection before this change; equals current for a watch seed.
        WindowState current; ///< Historical committed selection; the live source can already be newer.
        bool seed = false; ///< Watch initialization, rather than a source transition.
    };
    /** Owned simulation selection fact, committed before its dependent update. */
    struct SimulationViewChanged {
        SimulationView previous; ///< Previous selected view; equals current for a watch seed.
        SimulationView current; ///< Committed copied dimensions, without a live Window pointer.
        bool seed = false; ///< Initial callback for a newly attached required watch.
    };
    /** Optional observations; required sizing uses the corresponding source watch. */
    inline const CE::Events::EventChannel<WindowStateChanged> window_state_changed_event{"window-state-changed"};
    /** Optional simulation-owner view observations, with no implicit queued delivery. */
    inline const CE::Events::EventChannel<SimulationViewChanged> simulation_view_changed_event{"simulation-view-changed"};

    /** Owns an immediate required watch. Reset acknowledges invalidation and waits
     * for an entered callback, except when called by that callback itself.
     * Callback captures survive entry; no callback is made during destruction.
     */
    class ViewAttachment final {
        std::shared_ptr<Detail::ViewBinding> binding_;
        friend class RuntimeServices;
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
     * Retained source copies own no Context/window/native objects. Closure leaves
     * snapshots readable and rejects new watches without publishing a fake change.
     * @see [Observation and participation](cheryl-guide:runtime/event-delivery#observe-a-change-or-participate-in-an-operation)
     */
    template <class Snapshot, class Change> class ViewSource final {
        std::shared_ptr<Detail::ViewSourceState<Snapshot, Change>> state_;
        friend class Context;
        friend struct Detail::ViewSourceAccess;
        CE_API explicit ViewSource(CE::Events::EventBus& events);

    public:
        /** Copy synchronized current state from any thread, including before initialization and after closure. */
        [[nodiscard]] CE_API Snapshot snapshot() const;
        /** Attach on the initialized owner; seed runs before return. Empty callback/foreign owner/closure rejects.
         * A failed seed detaches and propagates. Later required failure prevents dependent runtime work;
         * retain the returned attachment and reset it before releasing borrowed targets.
         */
        [[nodiscard]] CE_API ViewAttachment watch(std::function<void(const Change&)> callback) const;
    };
    /** Retained read/watch facade for Context's platform-owned window selection. */
    using WindowStateSource = ViewSource<WindowState, WindowStateChanged>;
    /** Retained read/watch facade for Context's simulation-owned selected dimensions. */
    using SimulationViewSource = ViewSource<SimulationView, SimulationViewChanged>;
    extern template class CE_TYPE_API ViewSource<WindowState, WindowStateChanged>;
    extern template class CE_TYPE_API ViewSource<SimulationView, SimulationViewChanged>;
}
