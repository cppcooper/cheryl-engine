#pragma once

#include "input-record.h"
#include "input-routing.h"

#include <atomic>
#include <memory>
#include <vector>

namespace CE::Input {
    namespace Detail {
        struct CaptureCounts {
            std::atomic<std::size_t> events{0};
            std::atomic<std::size_t> text{0};
        };
    }

    /** A scoped request to collect a channel. Multiple requests coexist;
     * releasing a handle never erases already captured records or another request.
     * Move/release on any thread with exclusive access to this particular handle.
     * Retained request state survives collector destruction; it does not retain an adapter.
     */
    class CaptureLease final {
        friend class InputCapture;

    public:
        CaptureLease() = default;
        ~CaptureLease();
        CaptureLease(CaptureLease&& other) noexcept;
        CaptureLease& operator=(CaptureLease&& other) noexcept;
        CaptureLease(const CaptureLease&) = delete;
        CaptureLease& operator=(const CaptureLease&) = delete;
        void reset() noexcept;

    private:
        CaptureLease(std::shared_ptr<Detail::CaptureCounts> counts, InputMode mode);
        std::shared_ptr<Detail::CaptureCounts> counts_;
        InputMode mode_ = InputMode::State;
    };

    /** Platform-owned record collector; capture requests may come from any thread.
     * begin_poll() latches activation for the next pump. Collection/publication
     * stay on the platform thread and do not invoke consumers there.
     */
    class InputCapture final {
    public:
        InputCapture() = default;
        InputCapture(const InputCapture&) = delete;
        InputCapture& operator=(const InputCapture&) = delete;
        // Concurrent requests are supported; invalid modes throw. State is a no-op request.
        [[nodiscard]] CaptureLease request(InputMode mode);
        void begin_poll(KeyboardFocus focus = {});
        // Owns data in collector order for active channels only. Captured Text must
        // be a Unicode scalar; observation timestamps and other payloads are not normalized.
        void record(DeviceId device, DeviceKind kind, InputRecordData data, InputClock::time_point observed_at = InputClock::now());
        // Transfer the whole pending vector; discard does not revoke outstanding leases.
        [[nodiscard]] std::vector<InputRecord> complete();
        void discard_pending();

    private:
        std::shared_ptr<Detail::CaptureCounts> counts_ = std::make_shared<Detail::CaptureCounts>();
        bool events_enabled_ = false;
        bool text_enabled_ = false;
        std::uint64_t next_sequence_ = 1;
        std::vector<InputRecord> pending_;
        KeyboardFocus focus_;
    };
}
