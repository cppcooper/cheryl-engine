#pragma once

#include "input-bindings.h"
#include "input-capture.h"
#include "poll-snapshot.h"

#include <memory>

namespace CE::Platform {
    class Window;
}

namespace CE::Input {
    /** Platform-thread adapter. State is always available; supported Events/Text
     * channels are independently requested through scoped capture handles.
     * Backends latch capture before pumping and publish the complete poll once.
     * The initialized window is borrowed and must survive callback detachment.
     * Bindings, device queries and polling belong to the platform owner; capture/
     * focus requests and acquired immutable handles may cross threads.
     */
    class System {
    public:
        virtual ~System() = default;
        virtual void initialize(CE::Platform::Window& window) = 0;
        virtual void poll() = 0;
        virtual void deinitialize() = 0;
        // Borrowed mutable mapper; do not change it from simulation or retain past adapter teardown.
        [[nodiscard]] virtual Actions::Bindings& bindings() = 0;
        [[nodiscard]] virtual std::shared_ptr<const Actions::Snapshot> action_snapshot() { return bindings().action_snapshot(); }
        [[nodiscard]] virtual DeviceId keyboard_id() const = 0;
        [[nodiscard]] virtual DeviceId mouse_id() const = 0;
        [[nodiscard]] virtual DeviceId gamepad_id() const = 0;
        [[nodiscard]] virtual bool supports(Capture::Channel mode) const { return mode == Capture::Channel::State; }
        [[nodiscard]] virtual bool supports_focus() const { return false; }
        // Unsupported channels/focus throw failed_operation. Leases retain request
        // state, not the adapter/window, and never consume a private record queue.
        [[nodiscard]] Capture::Lease capture(Capture::Channel mode);
        [[nodiscard]] Routing::Manager& routing();
        // Retrieve after completed publication on the platform owner. State-only
        // adapters get empty records; capture-capable State-only publication throws.
        [[nodiscard]] virtual std::shared_ptr<const PollSnapshot> poll_snapshot();

    protected:
        // Before pumping, latch one focus/capture snapshot for routing and State gating.
        void begin_input_poll();
        [[nodiscard]] Capture::Collector& capture_buffer() { return capture_; }
        [[nodiscard]] std::shared_ptr<const PollSnapshot> publish_input(ObservationClock::time_point observed_at = ObservationClock::now());
        // Detachment cleanup drops pending records/focus, preserving delivered handles
        // and live capture request counts. Collector staging has no rollback guarantee.
        void discard_captured_input();

    private:
        Capture::Collector capture_;
        Routing::Manager routing_;
        std::atomic<std::shared_ptr<const PollSnapshot>> published_poll_{nullptr};
    };
}
