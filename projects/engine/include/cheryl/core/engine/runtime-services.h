#pragma once

#include "view-state.h"
#include <core/game-framework/simulation-scheduler.h>
#include <utility>

namespace CE::Runtime {
    namespace Detail {
        struct RuntimeServicesState;
        struct RuntimeServicesAccess;
    }
    /** Purpose of a platform pass, independent of new input or frame admission. */
    enum class MaintenanceReason {
        Running, ///< Ordinary initialized platform service opportunity, including startup waiting.
        ShutdownDrain, ///< Platform dependencies are being serviced while accepted work settles.
        FinalCollection ///< Frame/game releases finished; direct final collection follows when the renderer is ready.
    };
    /** Platform service boundary; it never pumps native input implicitly. */
    struct PlatformMaintenance {
        std::uint64_t context = 0; ///< Checked Context identity, separate from diagnostic counters.
        std::uint64_t session = 0; ///< Admitted runtime session identity.
        std::uint64_t pass = 0; ///< Checked increasing pass number within this session.
        std::chrono::steady_clock::time_point observed_at{}; ///< Owner's observation time for this pass.
        MaintenanceReason reason = MaintenanceReason::Running; ///< Running, dependency drainage or final collection.
        bool operator==(const PlatformMaintenance&) const = default;
    };
    /** One attempted step after input/view selection, before the game hook.
     * It owns no borrowed input and does not prove the game update completed.
     */
    struct SimulationStepStarting {
        std::uint64_t context = 0; ///< Checked Context identity.
        std::uint64_t session = 0; ///< Admitted session identity.
        std::uint64_t step = 0; ///< Checked attempted-step number, not a completed-update count.
        double delta_seconds = 0; ///< Selected finite nonnegative simulation seconds for this step.
        Simulation::UpdateKind kind = Simulation::UpdateKind::Variable; ///< Variable, fixed or recovery step selected by timing policy.
        double dropped_seconds = 0; ///< Finite nonnegative discarded simulation lag reported for this step.
        std::uint64_t view_revision = 0; ///< Selected SimulationView revision before this phase.
        bool operator==(const SimulationStepStarting&) const = default;
    };
    /** Optional platform observations; required module work uses on_maintenance(). */
    inline const CE::Events::EventChannel<PlatformMaintenance> platform_maintenance_event{"platform-maintenance"};
    /** Optional pre-update observations; publication does not prove the game hook ran. */
    inline const CE::Events::EventChannel<SimulationStepStarting> simulation_step_starting_event{"simulation-step-starting"};

    /** Owned immediate module participation; reset acknowledges invalidation.
     * Attachments added during a phase begin with the next phase, with no replay.
     */
    class ServiceAttachment final {
        ViewAttachment value_;
        friend class RuntimeServices;
        explicit ServiceAttachment(ViewAttachment value) noexcept
        : value_(std::move(value)) {}
    public:
        ServiceAttachment() = default;
        ~ServiceAttachment() = default;
        ServiceAttachment(ServiceAttachment&&) noexcept = default;
        ServiceAttachment& operator=(ServiceAttachment&&) noexcept = default;
        ServiceAttachment(const ServiceAttachment&) = delete;
        ServiceAttachment& operator=(const ServiceAttachment&) = delete;
        void reset() noexcept { value_.reset(); }
        [[nodiscard]] bool attached() const noexcept { return value_.attached(); }
    };

    /** Retained attachment access to Context's two runtime service owners.
     * Registration requires the initialized owner. Required callbacks finish
     * before dependent work; all are attempted before propagating the first error.
     * Ordinary observations use the typed bus channels instead.
     * Copies retain attachment coordination, not Context/native lifetime. New
     * attachment rejects after runtime closure; no destructor invokes callbacks.
     * @see [Runtime service phases](cheryl-guide:runtime/runtime-frame-boundary#runtime-service-phases)
     */
    class RuntimeServices final {
        std::shared_ptr<Detail::RuntimeServicesState> state_;
        friend class Context;
        friend struct Detail::RuntimeServicesAccess;
        CE_API explicit RuntimeServices(CE::Events::EventBus&);
    public:
        [[nodiscard]] CE_API ServiceAttachment on_maintenance(std::function<void(const PlatformMaintenance&)>) const;
        [[nodiscard]] CE_API ServiceAttachment on_step(std::function<void(const SimulationStepStarting&)>) const;
    };
}

/** @fn CE::Runtime::ServiceAttachment CE::Runtime::RuntimeServices::on_maintenance(std::function<void(const PlatformMaintenance&)> callback) const
 * Attach required immediate work on the initialized platform owner, normally from game init_platform().
 * No seed/replay is supplied. A callback added during a pass starts on the next pass. All required
 * participants are attempted before the first failure propagates. This phase never pumps native input.
 */
/** @fn CE::Runtime::ServiceAttachment CE::Runtime::RuntimeServices::on_step(std::function<void(const SimulationStepStarting&)> callback) const
 * Attach required work on the initialized simulation owner, normally from game init_simulation().
 * Registration during initialization participates from the first simulation step.
 * Future callbacks run after input/view selection and before the game hook. No current-step replay
 * occurs. Failure or a stop request prevents the dependent update; retain the attachment for lifetime.
 */
