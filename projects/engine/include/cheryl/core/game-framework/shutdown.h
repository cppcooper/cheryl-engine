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

    /** Final ordinary-request result; a failed endpoint call can reject synchronously instead. */
    enum class ShutdownDisposition {
        Accepted, ///< Platform accepted this request and committed stop.
        Cancelled, ///< Ordinary request was declined/withdrawn; the session may continue.
        Superseded, ///< Forced stop/failure displaced the unresolved request.
        Rejected ///< Request could not be admitted or the endpoint closed first.
    };
    /** Detail for a cancelled/rejected ticket, independent of forced shutdown cause. */
    enum class ShutdownCancellation {
        None, ///< No cancellation detail for acceptance/supersession.
        HostRejected, ///< Policy explicitly cancelled.
        PolicyDetached, ///< Policy authority was withdrawn while unresolved.
        PolicyFailed, ///< Policy callback or delivery failed.
        AdmissionFailed, ///< Owner admission could not complete.
        RuntimeClosed ///< Endpoint closed before an unresolved request completed.
    };
    /** Negotiation state; Pending keeps ordinary session work running. */
    enum class ShutdownStatus {
        Running, ///< No admitted ordinary request; staged work can still await its safe pass.
        Pending, ///< One request is admitted and awaiting a decision.
        Committed, ///< Stop is committed, before simulation/quiescence settlement.
        Quiescing, ///< Simulation/mailbox stopped and producer quiescence attempted.
        Stopped ///< Endpoint closed after the session or Runtime lifetime ended.
    };

    /** Owned settlement of one ticket; retained futures remain valid after Runtime destruction. */
    struct ShutdownResult {
        std::uint64_t request = 0; ///< Matching checked request identity.
        ShutdownDisposition disposition = ShutdownDisposition::Rejected; ///< Accepted/cancelled/superseded/rejected.
        ShutdownCancellation cancellation = ShutdownCancellation::None; ///< Cancellation/rejection detail when applicable.
    };
    /** Duplicate requests share this ID/result until a decision or forced stop. */
    struct ShutdownTicket {
        ShutdownRequest request; ///< Shared ordinary-request identity and origin.
        std::shared_future<ShutdownResult> result; ///< Settles on decision processing, forced stop or closure; do not block a needed owner.
    };
    /** Synchronized current negotiation state, distinct from the runtime's milestone stage. */
    struct ShutdownSnapshot {
        ShutdownStatus status = ShutdownStatus::Running; ///< Current controller state.
        std::optional<ShutdownRequest> request; ///< Staged or admitted unresolved request, if any.
        bool admitted = false; ///< False while initialization/owner processing has not admitted Pending.
        bool closed = false; ///< New endpoint commands reject; retained snapshots/results remain readable.
        ShutdownCause cause = ShutdownCause::None; ///< Committed cause, independent of request origin.
    };

    /** Copyable command access, without a borrowed Runtime. Requests stage for
     * a safe platform pass; closure rejects. Native input capacity does not gate it.
     * Do not wait for a result on an owner needed by the policy.
     * @see [Negotiable shutdown](cheryl-guide:runtime/runtime-frame-boundary#negotiable-shutdown)
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
    /** Capability policy chosen once before run; it does not change host request semantics. */
    struct ShutdownPolicyOptions {
        NativeClosePolicy native_close = NativeClosePolicy::Negotiate; ///< Default requires consumable native close; Force retains unconditional native close.
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

    /** Ordinary request was admitted Pending; observation contains no decision authority. */
    struct ShutdownRequested {
        RuntimeLifecycleSnapshot current; ///< Owned lifecycle selection at admission.
        ShutdownRequest request; ///< Admitted ordinary request.
    };
    /** An ordinary request was cancelled; forced supersession does not fabricate this event. */
    struct ShutdownRequestCancelled {
        RuntimeLifecycleSnapshot current; ///< Lifecycle selection after cancellation.
        ShutdownRequest request; ///< Cancelled identity; a future fresh request receives another ID.
        ShutdownCancellation reason = ShutdownCancellation::HostRejected; ///< Explicit rejection, withdrawal or policy failure detail.
    };
    inline const CE::Events::EventChannel<ShutdownRequested> shutdown_requested_event{"shutdown-requested"};
    inline const CE::Events::EventChannel<ShutdownRequestCancelled> shutdown_request_cancelled_event{"shutdown-request-cancelled"};
}

/** @fn CE::Runtime::ShutdownTicket CE::Runtime::ShutdownEndpoint::request(ShutdownReason reason) const
 * Stage an ordinary HostRequest or MainWindowClose for a safe platform pass and wake that owner.
 * Duplicate unresolved requests share the ticket ID/future. Invalid reason, empty/closed endpoint,
 * committed shutdown or exhausted identity space rejects before creating another admitted request.
 * With no installed policy, admission automatically accepts after Requested fanout returns.
 */
/** @fn CE::Runtime::ShutdownSnapshot CE::Runtime::ShutdownEndpoint::snapshot() const
 * Copy synchronized negotiation state from any thread. An empty endpoint reports closed/Stopped.
 * admitted distinguishes a staged request from one whose Pending observation has committed.
 */
/** @fn bool CE::Runtime::ShutdownDecision::accept() const
 * Stage acceptance for the matching active request/policy generation, from any thread. True means
 * the response was staged, not that shutdown already committed. Empty/stale/forced/closed responses
 * return false; the public request payload alone cannot construct this authority.
 */
/** @fn bool CE::Runtime::ShutdownDecision::cancel() const
 * Stage HostRejected for this matching active request. False means stale/empty/forced/closed or
 * already decided. Platform processing settles the ticket; no caller waits for owner work here.
 */
/** @fn void CE::Runtime::ShutdownPolicy::reset() noexcept
 * Withdraw authority, prevent fresh callback entry and acknowledge entered callbacks except self-reset.
 * An unresolved admitted ordinary request is cancelled. Owned callback captures stay pinned during
 * invocation. Reset before destroying borrowed policy/UI targets; default/moved-from handles are inert.
 */
