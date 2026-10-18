#pragma once

#include <cheryl/export.h>

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
        CE_API ~Lease();
        CE_API Lease(Lease&& other) noexcept;
        CE_API Lease& operator=(Lease&& other) noexcept;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        CE_API void reset() noexcept;

    private:
        CE_API Lease(std::shared_ptr<Detail::Counts> counts, Channel mode);
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
        [[nodiscard]] CE_API Lease request(Channel mode);
        CE_API void begin_poll(CE::Input::Routing::Focus focus = {});
        // Owns data in collector order for active channels only. Captured Text must
        // be a Unicode scalar; observation timestamps and other payloads are not normalized.
        CE_API void record(
            CE::Input::DeviceId device,
            CE::Input::DeviceKind kind,
            CE::Input::Events::Payload data,
            CE::Input::ObservationClock::time_point observed_at = CE::Input::ObservationClock::now()
        );
        // Transfer the whole pending vector; discard does not revoke outstanding leases.
        [[nodiscard]] CE_API std::vector<CE::Input::Events::Record> complete();
        CE_API void discard_pending();

    private:
        std::shared_ptr<Detail::Counts> counts_ = std::make_shared<Detail::Counts>();
        bool events_enabled_ = false;
        bool text_enabled_ = false;
        std::uint64_t next_sequence_ = 1;
        std::vector<CE::Input::Events::Record> pending_;
        CE::Input::Routing::Focus focus_;
    };
}

/** @fn CE::Input::Capture::Lease::Lease()
 * Create an empty lease requesting no channel.
 */

/** @fn CE::Input::Capture::Lease::~Lease()
 * Release this channel request without erasing pending/published records or another request.
 */

/** @fn CE::Input::Capture::Lease::Lease(Lease&& other) noexcept
 * Transfer retained channel-request state, leaving other empty. Handle access must be exclusive.
 */

/** @fn CE::Input::Capture::Lease& CE::Input::Capture::Lease::operator=(Lease&& other) noexcept
 * Reset this request then transfer other; no collector/adapter lifetime is retained.
 */

/** @fn void CE::Input::Capture::Lease::reset() noexcept
 * Idempotently release this request from any thread with exclusive handle access. Counts affect the next begin_poll latch, not already captured records.
 */

/** @fn CE::Input::Capture::Collector::Collector()
 * Create empty capture staging with independent shared request counts. Allocation may throw; recording/publication stays on the platform owner.
 */

/** @fn CE::Input::Capture::Lease CE::Input::Capture::Collector::request(Channel mode)
 * Request Events/Text concurrently with other producers. State is always collected and its lease is a no-op request; invalid modes throw invalid_args. Returned state can outlive this collector without retaining its adapter.
 */

/** @fn void CE::Input::Capture::Collector::begin_poll(CE::Input::Routing::Focus focus)
 * Latch current request counts and copied focus for the next platform pump. Requests/focus changes during that pump affect a later latch; existing pending records are not cleared.
 */

/** @fn void CE::Input::Capture::Collector::record(CE::Input::DeviceId device, CE::Input::DeviceKind kind, CE::Input::Events::Payload data, CE::Input::ObservationClock::time_point observed_at)
 * Append an owned active-channel payload in collector order. Inactive channels ignore records; active Text must be a Unicode scalar. Keyboard/Text routing uses latched target/epoch; PassThrough admits gameplay too. Observation time/other payloads are not normalized. Invalid Text/allocation failure throws; staging is not transactional.
 */

/** @fn std::vector<CE::Input::Events::Record> CE::Input::Capture::Collector::complete()
 * Transfer the whole pending vector in collector order and leave it empty; channel request counts and sequence history remain.
 */

/** @fn void CE::Input::Capture::Collector::discard_pending()
 * Drop staged records and clear the current activation/focus latch without revoking leases. The next begin_poll() relatches live request counts.
 * @see [Ordered capture](cheryl-guide:runtime/input-state-model#ordered-events-and-os-text)
 */
