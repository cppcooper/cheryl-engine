#pragma once

#include <cheryl/export.h>
#include <core/diagnostics.h>
#include <core/subsystems/event-channel.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string_view>

namespace CE::Runtime {
    class GameRuntime;
    namespace Detail {
        struct LifecycleState;
        class ShutdownController;
    }

    /** Completed/unstarted session observations, copied after synchronization; never a live admission policy. */
    struct RuntimeStats {
        ::CE::Diagnostics::DomainId domain = 0; ///< Process-local runtime identity.
        std::uint64_t updates = 0;              ///< Actual game update invocations.
        std::uint64_t polls = 0;                ///< Completed platform input polls.
        std::uint64_t published = 0;            ///< Completed render frames published for consumption.
        std::uint64_t rendered = 0;             ///< Frames consumed by the renderer.
        std::uint64_t superseded = 0;           ///< Published frames replaced before rendering.
        std::uint64_t skipped_publication = 0;  ///< Frame preparations skipped because no publication slot was free.
        std::uint64_t dropped_batches = 0;      ///< Recovery batches with discarded simulation lag.
        std::uint64_t dropped_nanoseconds = 0;  ///< Total simulation lag discarded by scheduling policy, in nanoseconds.
        std::uint64_t peak_polls = 0;           ///< Maximum retained poll backlog observed.
        std::uint64_t resizes = 0;              ///< Observed framebuffer-size changes.
        std::uint64_t input_records = 0;        ///< Ordered input records observed across published polls.
        std::uint64_t focus_changes = 0;        ///< Observed focus-ownership epoch changes.
        std::uint64_t backpressure = 0;         ///< Observed cycles where bounded input admission prevented another poll.
    };

    /** Session milestones; pending shutdown negotiation is tracked separately. */
    enum class RuntimeStage {
        Unstarted, ///< No session has been admitted.
        Initializing, ///< Session reserved; startup capabilities can be partial.
        Ready, ///< Game initialization and initial owners/views established before the first update.
        Stopping, ///< Shutdown committed; a simulation hook can still be finishing.
        Quiescing, ///< Simulation/mailbox stopped and applicable producer quiescence attempted.
        Stopped ///< Ordered cleanup attempted and terminal outcome retained.
    };
    /** Why shutdown committed, independent of an ordinary request's user-facing reason. */
    enum class ShutdownCause {
        None, ///< Shutdown has not committed.
        StopRequested, ///< Unconditional host/tick stop.
        WindowClose, ///< Native close forced shutdown without negotiation.
        Failure, ///< Runtime execution failed.
        AcceptedRequest ///< An ordinary host/native-close request was accepted.
    };
    /** Origin of an ordinary negotiable request. */
    enum class ShutdownReason { HostRequest, ///< Explicit saved endpoint request.
        MainWindowClose ///< Main-window close intent entered the runtime bridge.
    };
    /** Owned request identity and origin, with no decision authority. */
    struct ShutdownRequest {
        std::uint64_t id = 0; ///< Checked process-local request identity; duplicates share it while unresolved.
        ShutdownReason reason = ShutdownReason::HostRequest; ///< Host or main-window origin.
        std::chrono::steady_clock::time_point requested_at{}; ///< Time the request was staged.
    };
    /** Retained direct-session result, finalized after cleanup. */
    enum class RuntimeOutcome { Pending, ///< No terminal result yet.
        Completed, ///< Run/cleanup completed without a primary exception.
        Failed ///< A primary runtime/startup/cleanup failure is retained.
    };

    /** Successfully established startup capabilities, including partial startup. */
    struct InitializedCapabilities {
        bool renderer = false; ///< Renderer initialization completed successfully.
        bool input = false; ///< Input initialization completed successfully.
        bool game = false; ///< Both game initialization hooks completed successfully.
        bool simulation_owner = false; ///< Simulation owner/bootstrap was established.
    };

    /** Bounded owned diagnostic text; it never borrows the original exception. */
    struct RuntimeFailure {
        std::array<char, 64> phase{}; ///< NUL-terminated owned phase summary.
        std::array<char, 192> message{}; ///< NUL-terminated owned exception summary.
        bool present = false; ///< A failure was captured; false means no summary.
        bool truncated = false; ///< At least one text field exceeded retained capacity.
        /** Borrow this summary's phase text; keep the RuntimeFailure value alive. */
        [[nodiscard]] std::string_view phase_text() const noexcept { return phase.data(); }
        /** Borrow this summary's message text; keep the RuntimeFailure value alive. */
        [[nodiscard]] std::string_view message_text() const noexcept { return message.data(); }
    };

    /** Current milestone and retained terminal outcome, safe after Runtime destruction.
     * Stopped means ordered cleanup was attempted. A failed cleanup is recorded;
     * it does not establish successful native resource release.
     */
    struct RuntimeLifecycleSnapshot {
        std::uint64_t context = 0; ///< Checked Context identity; zero before session admission.
        std::uint64_t session = 0; ///< Checked single-session identity.
        std::uint64_t sequence = 0; ///< Advances for milestones and request admission/cancellation; not necessarily consecutive per channel.
        std::chrono::steady_clock::time_point observed_at{}; ///< Time of the latest committed lifecycle fact.
        RuntimeStage stage = RuntimeStage::Unstarted; ///< Latest selected milestone.
        ShutdownCause cause = ShutdownCause::None; ///< Committed shutdown cause, if any.
        bool reached_ready = false; ///< Ready was reached before shutdown; remains true during teardown.
        bool stop_requested = false; ///< Shared unconditional/committed stop has been requested.
        bool closed = false; ///< Terminal source closure; observation remains readable.
        InitializedCapabilities initialized; ///< Successful startup milestones, including partial startup.
        std::optional<ShutdownRequest> pending_request; ///< Admitted ordinary request awaiting a decision.
        std::optional<ShutdownRequest> committed_request; ///< Accepted request that committed shutdown, when applicable.
        std::optional<ShutdownRequest> superseded_request; ///< Ordinary request displaced by forced stop/failure.
        RuntimeOutcome outcome = RuntimeOutcome::Pending; ///< Terminal completion/failure, finalized at Stopped.
        RuntimeFailure primary_failure; ///< Owned primary exception summary; original direct exception still propagates.
        std::array<RuntimeFailure, 8> secondary_failures{}; ///< First bounded cleanup/secondary failure summaries.
        std::uint64_t secondary_failure_count = 0; ///< Total; entries beyond the fixed array are omitted.
        RuntimeStats stats; ///< Final counters populated at Stopped; not a live scheduler admission policy.
    };

    /** Platform milestone before the first update. Immediate observers must not
     * synchronously wait for the simulation owner, which is still gated.
     */
    struct RuntimeReady {
        RuntimeLifecycleSnapshot current; ///< Owned lifecycle selection for this milestone.
    };
    /** Committed shutdown before simulation join; partial startup is included. */
    struct ShutdownStarting {
        RuntimeLifecycleSnapshot current; ///< Owned lifecycle selection for this milestone.
    };
    /** Simulation/mailbox stopped and any applicable quiesce attempted, before accepted CPU work settles. */
    struct RuntimeQuiescing {
        RuntimeLifecycleSnapshot current; ///< Owned lifecycle selection for this milestone.
    };
    /** Terminal cleanup attempt and outcome; does not guarantee successful native release. */
    struct RuntimeStopped {
        RuntimeLifecycleSnapshot current; ///< Owned lifecycle selection for this milestone.
    };
    inline const CE::Events::EventChannel<RuntimeReady> runtime_ready_event{"runtime-ready"};
    inline const CE::Events::EventChannel<ShutdownStarting> shutdown_starting_event{"shutdown-starting"};
    inline const CE::Events::EventChannel<RuntimeQuiescing> runtime_quiescing_event{"runtime-quiescing"};
    inline const CE::Events::EventChannel<RuntimeStopped> runtime_stopped_event{"runtime-stopped"};

    /** Retained synchronized observation, without a borrowed Runtime or adapter.
     * @see [Lifecycle observation](cheryl-guide:runtime/runtime-frame-boundary#lifecycle-observation)
     */
    class RuntimeLifecycle final {
        std::shared_ptr<Detail::LifecycleState> state_;
        friend class GameRuntime;
        friend class Detail::ShutdownController;
        CE_API RuntimeLifecycle();
        CE_API void begin(std::uint64_t context, std::uint64_t session);
        CE_API void request_stop();
        CE_API std::optional<RuntimeLifecycleSnapshot> ready(InitializedCapabilities, bool stopped);
        CE_API std::optional<RuntimeLifecycleSnapshot> starting(InitializedCapabilities, ShutdownCause);
        CE_API std::optional<RuntimeLifecycleSnapshot> quiescing();
        CE_API RuntimeLifecycleSnapshot pending(std::optional<ShutdownRequest>, InitializedCapabilities);
        CE_API RuntimeLifecycleSnapshot stopped(const RuntimeStats&, const char* phase, std::exception_ptr);
        CE_API void secondary(const char* phase, std::exception_ptr) noexcept;
        CE_API void close() noexcept;

    public:
        /** Copy current state from any thread, including before run and after Runtime destruction.
         * Bus subscriptions have no replay; terminal state owns failure text and final counters.
         */
        [[nodiscard]] CE_API RuntimeLifecycleSnapshot snapshot() const;
    };
}
