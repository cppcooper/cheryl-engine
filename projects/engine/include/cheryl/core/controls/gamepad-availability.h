#pragma once

#include "input-record.h"
#include <cheryl/core/subsystems/event-channel.h>

#include <cstdint>

namespace CE::Input {
    /** Copied observation of one adapter's convenience slot, not physical hardware identity.
     * Register on the publication owner, then read System::gamepad_availability()
     * for a coherent seed. Retained copies contain no adapter/native pointers.
     */
    struct GamepadAvailability {
        std::uint64_t source = 0;   ///< Checked attachment identity; zero before a source opens.
        std::uint64_t slot = 0;     ///< Checked slot identity retained across this adapter's attachments.
        DeviceId device{};          ///< Adapter-local convenience device ID; meaningful once known.
        std::uint64_t revision = 0; ///< Source revision; zero is the unknown initial baseline.
        std::uint64_t epoch = 0;    ///< Sampled availability epoch, advanced on each unavailable/unknown-to-available transition.
        std::uint64_t poll = 0;     ///< Completed semantic poll associated with the last availability change.
        ObservationClock::time_point observed_at{}; ///< That poll's observation time, or explicit source-close time.
        bool known = false;                         ///< Whether any complete poll has established presence.
        bool available = false;                     ///< Sampled convenience-slot presence when known; closure preserves this value.
        bool closed = true;                         ///< Source ended; this does not report a physical disconnect.
    };

    /** Latest committed availability and the preceding published observation.
     * Reentrant changes coalesce until the next owner publication. Revisions and
     * epochs expose skipped state; queued consumers reject older/equal revisions.
     */
    struct GamepadAvailabilityChanged {
        GamepadAvailability current;  ///< Owned committed state.
        GamepadAvailability previous; ///< Last observation selected for publication, possibly older after coalescing.
    };

    /** Platform-owned convenience-slot observation, published after complete input polling. */
    inline const CE::Events::EventChannel<GamepadAvailabilityChanged> gamepad_availability_changed_event{"gamepad-availability-changed"};
}
