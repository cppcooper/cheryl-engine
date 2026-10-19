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

    enum class RuntimeStage { Unstarted, Initializing, Ready, Stopping, Quiescing, Stopped };
    enum class ShutdownCause { None, StopRequested, WindowClose, Failure, AcceptedRequest };
    enum class RuntimeOutcome { Pending, Completed, Failed };

    /** Successfully established startup capabilities, including partial startup. */
    struct InitializedCapabilities {
        bool renderer = false;
        bool input = false;
        bool game = false;
        bool simulation_owner = false;
    };

    /** Bounded owned diagnostic text; it never borrows the original exception. */
    struct RuntimeFailure {
        std::array<char, 64> phase{};
        std::array<char, 192> message{};
        bool present = false;
        bool truncated = false;
        [[nodiscard]] std::string_view phase_text() const noexcept { return phase.data(); }
        [[nodiscard]] std::string_view message_text() const noexcept { return message.data(); }
    };

    /** Current milestone and retained terminal outcome, safe after Runtime destruction.
     * Stopped means ordered cleanup was attempted. A failed cleanup is recorded;
     * it does not establish successful native resource release.
     */
    struct RuntimeLifecycleSnapshot {
        std::uint64_t context = 0;
        std::uint64_t session = 0;
        std::uint64_t sequence = 0;
        std::chrono::steady_clock::time_point observed_at{};
        RuntimeStage stage = RuntimeStage::Unstarted;
        ShutdownCause cause = ShutdownCause::None;
        bool reached_ready = false;
        bool stop_requested = false;
        bool closed = false;
        InitializedCapabilities initialized;
        RuntimeOutcome outcome = RuntimeOutcome::Pending;
        RuntimeFailure primary_failure;
        std::array<RuntimeFailure, 8> secondary_failures{};
        std::uint64_t secondary_failure_count = 0; ///< Total; entries beyond the fixed array are omitted.
        RuntimeStats stats;
    };

    /** Platform milestone before the first update. Immediate observers must not
     * synchronously wait for the simulation owner, which is still gated.
     */
    struct RuntimeReady {
        RuntimeLifecycleSnapshot current;
    };
    struct ShutdownStarting {
        RuntimeLifecycleSnapshot current;
    };
    struct RuntimeQuiescing {
        RuntimeLifecycleSnapshot current;
    };
    struct RuntimeStopped {
        RuntimeLifecycleSnapshot current;
    };
    inline const CE::Events::EventChannel<RuntimeReady> runtime_ready_event{"runtime-ready"};
    inline const CE::Events::EventChannel<ShutdownStarting> shutdown_starting_event{"shutdown-starting"};
    inline const CE::Events::EventChannel<RuntimeQuiescing> runtime_quiescing_event{"runtime-quiescing"};
    inline const CE::Events::EventChannel<RuntimeStopped> runtime_stopped_event{"runtime-stopped"};

    /** Retained synchronized observation, without a borrowed Runtime or adapter. */
    class RuntimeLifecycle final {
        std::shared_ptr<Detail::LifecycleState> state_;
        friend class GameRuntime;
        CE_API RuntimeLifecycle();
        CE_API void begin(std::uint64_t context, std::uint64_t session);
        CE_API void request_stop();
        CE_API std::optional<RuntimeLifecycleSnapshot> ready(InitializedCapabilities, bool stopped);
        CE_API std::optional<RuntimeLifecycleSnapshot> starting(InitializedCapabilities, ShutdownCause);
        CE_API std::optional<RuntimeLifecycleSnapshot> quiescing();
        CE_API RuntimeLifecycleSnapshot stopped(const RuntimeStats&, const char* phase, std::exception_ptr);
        CE_API void secondary(const char* phase, std::exception_ptr) noexcept;
        CE_API void close() noexcept;

    public:
        [[nodiscard]] CE_API RuntimeLifecycleSnapshot snapshot() const;
    };
}
