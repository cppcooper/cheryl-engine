#pragma once

#include "input-record.h"
#include "input-routing.h"

#include <atomic>
#include <memory>
#include <vector>

namespace CE::Input::Capture {
    namespace Detail {
        struct Counts {
            std::atomic<std::size_t> events{0};
            std::atomic<std::size_t> text{0};
        };
    }

    /** A scoped request to collect a channel. Multiple requests coexist;
     * releasing a handle never erases already captured records or another request.
     * Move/release on any thread with exclusive access to this particular handle.
     * Retained request state survives collector destruction; it does not retain an adapter.
     */
    class Lease final {
        friend class Collector;

    public:
        Lease() = default;
        ~Lease();
        Lease(Lease&& other) noexcept;
        Lease& operator=(Lease&& other) noexcept;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        void reset() noexcept;

    private:
        Lease(std::shared_ptr<Detail::Counts> counts, Channel mode);
        std::shared_ptr<Detail::Counts> counts_;
        Channel mode_ = Channel::State;
    };

    /** Platform-owned record collector; capture requests may come from any thread.
     * begin_poll() latches activation for the next pump. Collection/publication
     * stay on the platform thread and do not invoke consumers there.
     */
    class Collector final {
    public:
        Collector() = default;
        Collector(const Collector&) = delete;
        Collector& operator=(const Collector&) = delete;
        // Concurrent requests are supported; invalid modes throw. State is a no-op request.
        [[nodiscard]] Lease request(Channel mode);
        void begin_poll(CE::Input::Routing::Focus focus = {});
        // Owns data in collector order for active channels only. Captured Text must
        // be a Unicode scalar; observation timestamps and other payloads are not normalized.
        void record(
            CE::Input::DeviceId device,
            CE::Input::DeviceKind kind,
            CE::Input::Events::Payload data,
            CE::Input::ObservationClock::time_point observed_at = CE::Input::ObservationClock::now()
        );
        // Transfer the whole pending vector; discard does not revoke outstanding leases.
        [[nodiscard]] std::vector<CE::Input::Events::Record> complete();
        void discard_pending();

    private:
        std::shared_ptr<Detail::Counts> counts_ = std::make_shared<Detail::Counts>();
        bool events_enabled_ = false;
        bool text_enabled_ = false;
        std::uint64_t next_sequence_ = 1;
        std::vector<CE::Input::Events::Record> pending_;
        CE::Input::Routing::Focus focus_;
    };
}
