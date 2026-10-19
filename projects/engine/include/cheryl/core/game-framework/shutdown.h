#pragma once

#include "runtime-lifecycle.h"
#include <core/subsystems/event-bus.h>

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <utility>

namespace CE::Runtime {
    namespace Detail {
        struct ShutdownState;
        struct ShutdownPolicyState;
        class ShutdownController;
    }

    enum class ShutdownDisposition { Accepted, Cancelled, Superseded, Rejected };
    enum class ShutdownCancellation { None, HostRejected, PolicyDetached, PolicyFailed, AdmissionFailed, RuntimeClosed };
    enum class ShutdownStatus { Running, Pending, Committed, Quiescing, Stopped };

    struct ShutdownResult {
        std::uint64_t request = 0;
        ShutdownDisposition disposition = ShutdownDisposition::Rejected;
        ShutdownCancellation cancellation = ShutdownCancellation::None;
    };
    /** Duplicate requests share this ID/result until a decision or forced stop. */
    struct ShutdownTicket {
        ShutdownRequest request;
        std::shared_future<ShutdownResult> result;
    };
    struct ShutdownSnapshot {
        ShutdownStatus status = ShutdownStatus::Running;
        std::optional<ShutdownRequest> request;
        bool admitted = false; ///< False while initialization/owner processing has not admitted Pending.
        bool closed = false;
        ShutdownCause cause = ShutdownCause::None;
    };

    /** Copyable command access, without a borrowed Runtime. Requests stage for
     * a safe platform pass; closure rejects. Native input capacity does not gate it.
     * Do not wait for a result on an owner needed by the policy.
     */
    class ShutdownEndpoint final {
        std::shared_ptr<Detail::ShutdownState> state_;
        friend class Detail::ShutdownController;
        explicit ShutdownEndpoint(std::shared_ptr<Detail::ShutdownState> state) noexcept
        : state_(std::move(state)) {}
    public:
        ShutdownEndpoint() = default;
        [[nodiscard]] CE_API ShutdownTicket request(ShutdownReason reason = ShutdownReason::HostRequest) const;
        [[nodiscard]] CE_API ShutdownSnapshot snapshot() const;
    };

    /** Private decision capability supplied only to the installed host policy.
     * Stale decisions and responses after force/closure return false.
     */
    class ShutdownDecision final {
        std::weak_ptr<Detail::ShutdownState> state_;
        std::uint64_t request_ = 0;
        std::uint64_t generation_ = 0;
        friend class Detail::ShutdownController;
        ShutdownDecision(std::weak_ptr<Detail::ShutdownState>, std::uint64_t request, std::uint64_t generation) noexcept;
    public:
        ShutdownDecision() = default;
        [[nodiscard]] CE_API bool accept() const;
        [[nodiscard]] CE_API bool cancel() const;
        [[nodiscard]] std::uint64_t request_id() const noexcept { return request_; }
    };

    /** Immediate policies run on platform and return without waiting for simulation. */
    using ShutdownPolicyCallback = std::function<void(const ShutdownRequest&, ShutdownDecision)>;
    /** Negotiate requires a consumable native close capability. Force keeps host
     * requests negotiable but makes native close unconditional, including while Pending.
     */
    enum class NativeClosePolicy { Negotiate, Force };
    struct ShutdownPolicyOptions {
        NativeClosePolicy native_close = NativeClosePolicy::Negotiate;
    };

    /** Owns the single host authority. Reset prevents new entry and acknowledges
     * running callbacks (except self-reset); withdrawal cancels an unresolved request.
     */
    class ShutdownPolicy final {
        std::shared_ptr<Detail::ShutdownPolicyState> state_;
        friend class Detail::ShutdownController;
        explicit ShutdownPolicy(std::shared_ptr<Detail::ShutdownPolicyState> state) noexcept
        : state_(std::move(state)) {}
    public:
        ShutdownPolicy() = default;
        CE_API ~ShutdownPolicy();
        CE_API ShutdownPolicy(ShutdownPolicy&&) noexcept;
        CE_API ShutdownPolicy& operator=(ShutdownPolicy&&) noexcept;
        ShutdownPolicy(const ShutdownPolicy&) = delete;
        ShutdownPolicy& operator=(const ShutdownPolicy&) = delete;
        CE_API void reset() noexcept;
        [[nodiscard]] CE_API bool active() const noexcept;
    };

    struct ShutdownRequested {
        RuntimeLifecycleSnapshot current;
        ShutdownRequest request;
    };
    struct ShutdownRequestCancelled {
        RuntimeLifecycleSnapshot current;
        ShutdownRequest request;
        ShutdownCancellation reason = ShutdownCancellation::HostRejected;
    };
    inline const CE::Events::EventChannel<ShutdownRequested> shutdown_requested_event{"shutdown-requested"};
    inline const CE::Events::EventChannel<ShutdownRequestCancelled> shutdown_request_cancelled_event{"shutdown-request-cancelled"};
}
