#pragma once

#include <cheryl/export.h>

#include "input-bindings.h"
#include "action-context.h"
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
    class CE_TYPE_API System {
    public:
        CE_API virtual ~System();
        virtual void initialize(CE::Platform::Window& window) = 0;
        virtual void poll() = 0;
        virtual void deinitialize() = 0;
        // Borrowed mutable mapper; do not change it from simulation or retain past adapter teardown.
        [[nodiscard]] virtual Actions::Bindings& bindings() = 0;
        // Acquire on platform; retain the endpoint to attach contexts from simulation.
        [[nodiscard]] Actions::ContextSubmission action_contexts() { return bindings().action_contexts(); }
        [[nodiscard]] virtual std::shared_ptr<const Actions::Snapshot> action_snapshot() { return bindings().action_snapshot(); }
        [[nodiscard]] virtual DeviceId keyboard_id() const = 0;
        [[nodiscard]] virtual DeviceId mouse_id() const = 0;
        [[nodiscard]] virtual DeviceId gamepad_id() const = 0;
        [[nodiscard]] virtual bool supports(Capture::Channel mode) const { return mode == Capture::Channel::State; }
        [[nodiscard]] virtual bool supports_focus() const { return false; }
        // Unsupported channels/focus throw failed_operation. Leases retain request
        // state, not the adapter/window, and never consume a private record queue.
        [[nodiscard]] CE_API Capture::Lease capture(Capture::Channel mode);
        [[nodiscard]] CE_API Routing::Manager& routing();
        // Retrieve after completed publication on the platform owner. State-only
        // adapters get empty records; capture-capable State-only publication throws.
        [[nodiscard]] CE_API virtual std::shared_ptr<const PollSnapshot> poll_snapshot();

    protected:
        // Before pumping, latch one focus/capture snapshot for routing and State gating.
        CE_API void begin_input_poll();
        [[nodiscard]] Capture::Collector& capture_buffer() { return capture_; }
        [[nodiscard]] CE_API std::shared_ptr<const PollSnapshot>
        publish_input(ObservationClock::time_point observed_at = ObservationClock::now());
        // Detachment cleanup drops pending records/focus, preserving delivered handles
        // and live capture request counts. Collector staging has no rollback guarantee.
        CE_API void discard_captured_input();

    private:
        Capture::Collector capture_;
        Routing::Manager routing_;
        std::atomic<std::shared_ptr<const PollSnapshot>> published_poll_{nullptr};
    };
}

/** @fn CE::Input::System::~System()
 * Destroy only after input users/borrowed window callbacks quiesce. Concrete adapters detach native callbacks before window destruction; retained capture/focus requests own request state only.
 */

/** @fn void CE::Input::System::initialize(CE::Platform::Window& window)
 * Attach a borrowed compatible native window on the platform owner. Concrete adapter validation/native failures can throw; window must survive detachment.
 */

/** @fn void CE::Input::System::poll()
 * Complete one platform-owned native pump and publish its semantic/capture state. Concrete adapters establish capture/focus before observations; callers do not poll from simulation.
 */

/** @fn void CE::Input::System::deinitialize()
 * Detach platform callbacks/resources while the borrowed window is still alive, preserving already delivered immutable state. Concrete adapters define failure cleanup; do not race polling or destruction.
 */

/** @fn CE::Input::Actions::Bindings& CE::Input::System::bindings()
 * Borrow the mutable mapper on the platform owner only; do not retain past adapter teardown or mutate from simulation.
 */

/** @fn CE::Input::Actions::ContextSubmission CE::Input::System::action_contexts()
 * Acquire the retained context submission endpoint on platform. Later attach calls may run from simulation/other producers without re-querying this adapter.
 */

/** @fn std::shared_ptr<const CE::Input::Actions::Snapshot> CE::Input::System::action_snapshot()
 * Retain the current immutable semantic snapshot through bindings(). Acquire through the adapter on its permitted owner; acquired handles can cross threads.
 */

/** @fn CE::Input::DeviceId CE::Input::System::keyboard_id() const
 * Return adapter-owned keyboard identity on platform; cache during initialization for later value-only mapping requests.
 */

/** @fn CE::Input::DeviceId CE::Input::System::mouse_id() const
 * Return adapter-owned mouse identity on platform; does not expose live native mouse state.
 */

/** @fn CE::Input::DeviceId CE::Input::System::gamepad_id() const
 * Return the adapter's single convenience gamepad identity on platform. This interface does not enumerate all native pads.
 */

/** @fn bool CE::Input::System::supports(Capture::Channel mode) const
 * Report capture capability; the base supports State only. Committed Text support does not promise IME composition.
 */

/** @fn bool CE::Input::System::supports_focus() const
 * Report routed keyboard-focus capability; base adapters return false.
 */

/** @fn CE::Input::Capture::Lease CE::Input::System::capture(Capture::Channel mode)
 * Retain a scoped request for a supported channel; unsupported channels throw failed_operation. Requests may cross threads after capability/adapter lifetime requirements are respected; leases do not retain the window or consume private queues.
 */

/** @fn CE::Input::Routing::Manager& CE::Input::System::routing()
 * Borrow the synchronized focus manager. Unsupported routing throws failed_operation; do not retain the reference beyond the adapter, but acquired leases retain safe request state.
 */

/** @fn std::shared_ptr<const CE::Input::PollSnapshot> CE::Input::System::poll_snapshot()
 * Retrieve on platform after publication. A capture-capable adapter must publish State+records together or this throws. State-only adapters may synthesize empty records around their latest state. Returned handle is independent immutable ownership.
 */

/** @fn void CE::Input::System::begin_input_poll()
 * Adapter hook before pumping: latch one focus/capture snapshot, gate keyboard gameplay State and open the action-context poll boundary. Mid-pump requests wait for the next poll.
 */

/** @fn CE::Input::Capture::Collector& CE::Input::System::capture_buffer()
 * Borrow platform-owned ordered capture staging for backend callbacks; never expose it as a simulation reader queue.
 */

/** @fn std::shared_ptr<const CE::Input::PollSnapshot> CE::Input::System::publish_input(ObservationClock::time_point observed_at)
 * Complete semantic State and pending records, then atomically publish a complete retained poll. Call once after pumping with nondecreasing observation time; allocation/evaluation errors propagate and no incomplete poll handle is returned.
 */

/** @fn void CE::Input::System::discard_captured_input()
 * Detach cleanup: drop pending records/focus/latest complete-poll ownership while preserving previously delivered handles and live capture request counts. Mapper clear/release publication is a separate adapter responsibility.
 * @see [Adapter publication](cheryl-guide:runtime/input-state-model#adapter-publication)
 */
