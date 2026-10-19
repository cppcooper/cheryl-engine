#pragma once

#include "view-state.h"
#include <core/game-framework/simulation-scheduler.h>
#include <utility>

namespace CE::Runtime {
    namespace Detail {
        struct RuntimeServicesState;
        struct RuntimeServicesAccess;
    }
    enum class MaintenanceReason { Running, ShutdownDrain, FinalCollection };
    /** Platform service boundary; it never pumps native input implicitly. */
    struct PlatformMaintenance {
        std::uint64_t context = 0;
        std::uint64_t session = 0;
        std::uint64_t pass = 0;
        std::chrono::steady_clock::time_point observed_at{};
        MaintenanceReason reason = MaintenanceReason::Running;
        bool operator==(const PlatformMaintenance&) const = default;
    };
    /** One attempted step after input/view selection, before the game hook.
     * It owns no borrowed input and does not prove the game update completed.
     */
    struct SimulationStepStarting {
        std::uint64_t context = 0;
        std::uint64_t session = 0;
        std::uint64_t step = 0;
        double delta_seconds = 0;
        Simulation::UpdateKind kind = Simulation::UpdateKind::Variable;
        double dropped_seconds = 0;
        std::uint64_t view_revision = 0;
        bool operator==(const SimulationStepStarting&) const = default;
    };
    inline const CE::Events::EventChannel<PlatformMaintenance> platform_maintenance_event{"platform-maintenance"};
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
