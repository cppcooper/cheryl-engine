#include "runtime-services-internal.h"
#include "required-bindings.h"
#include <internals/compile-time-logging.hpp>

#include <cmath>
#include <limits>

namespace CE::Runtime::Detail {
    struct RuntimeServicesState {
        CE::Events::EventBus* bus;
        CE::Events::EventBus::Publisher publisher;
        mutable std::mutex mutex;
        std::uint64_t context = 0;
        std::uint64_t session = 0;
        std::uint64_t pass = 0;
        std::uint64_t step = 0;
        std::thread::id platform_owner;
        std::thread::id simulation_owner;
        bool closed = false;
        std::optional<MaintenanceReason> previous_reason;
        RequiredBindings<PlatformMaintenance> maintenance;
        RequiredBindings<SimulationStepStarting> simulation;
        std::function<void()> before_maintenance;
        std::function<void()> before_step;

        explicit RuntimeServicesState(CE::Events::EventBus& events)
        : bus(&events), publisher(events.publisher()) {}
        void require_owner(std::thread::id owner) const {
            if (closed || !context || !session || owner != std::this_thread::get_id())
                throw CE::Exceptions::failed_operation(CE_HERE, "Service attachment/publication requires its initialized runtime owner");
        }
    };

    RuntimeServices RuntimeServicesAccess::make(CE::Events::EventBus& bus) {
        return RuntimeServices(bus);
    }
    void RuntimeServicesAccess::begin(RuntimeServices& services, std::uint64_t context, std::uint64_t session) {
        auto& state = *services.state_;
        std::lock_guard lock(state.mutex);
        state.context = context;
        state.session = session;
        state.platform_owner = std::this_thread::get_id();
    }
    void RuntimeServicesAccess::bind_simulation(RuntimeServices& services) {
        auto& state = *services.state_;
        std::lock_guard lock(state.mutex);
        if (state.closed || !state.context || !state.session || state.simulation_owner != std::thread::id{})
            throw CE::Exceptions::failed_operation(CE_HERE, "Simulation services bind once during initialized owner bootstrap");
        state.simulation_owner = std::this_thread::get_id();
    }
    void RuntimeServicesAccess::maintenance(RuntimeServices& services, MaintenanceReason reason) {
        auto& state = *services.state_;
        PlatformMaintenance phase;
        bool report = false;
        {
            std::lock_guard lock(state.mutex);
            state.require_owner(state.platform_owner);
            if (state.maintenance.publishing || state.pass == std::numeric_limits<std::uint64_t>::max())
                throw CE::Exceptions::failed_operation(CE_HERE, "Maintenance pass is reentrant or exhausted");
            phase = {state.context, state.session, ++state.pass, std::chrono::steady_clock::now(), reason};
            report = state.previous_reason != reason || phase.pass % 120 == 0;
            state.previous_reason = reason;
        }
        const auto result = state.maintenance.publish(phase, state.publisher, platform_maintenance_event, state.before_maintenance);
        if (report || result.failure || result.fallbacks)
            CE_LOG_DEBUG(
                CE::Logging::Categories::engine,
                "subsystem=events event=platform-maintenance operation=notification_attempt session={} pass={} reason={} required_entries={} fallbacks={} required_failed={} sample_period=120",
                phase.session, phase.pass,
                reason == MaintenanceReason::Running         ? "running"
                : reason == MaintenanceReason::ShutdownDrain ? "shutdown_drain"
                                                             : "final_collection",
                result.entered, result.fallbacks, bool(result.failure)
            );
        if (result.failure)
            std::rethrow_exception(result.failure);
    }
    void
    RuntimeServicesAccess::step(RuntimeServices& services, double delta, Simulation::UpdateKind kind, double dropped, std::uint64_t view) {
        if (!std::isfinite(delta) || delta < 0 || !std::isfinite(dropped) || dropped < 0)
            throw CE::Exceptions::invalid_args(CE_HERE, "Simulation service step requires finite nonnegative durations");
        auto& state = *services.state_;
        SimulationStepStarting phase;
        {
            std::lock_guard lock(state.mutex);
            state.require_owner(state.simulation_owner);
            if (state.simulation.publishing || state.step == std::numeric_limits<std::uint64_t>::max())
                throw CE::Exceptions::failed_operation(CE_HERE, "Simulation service step is reentrant or exhausted");
            phase = {state.context, state.session, ++state.step, delta, kind, dropped, view};
        }
        const auto result = state.simulation.publish(phase, state.publisher, simulation_step_starting_event, state.before_step);
        if (phase.step == 1 || phase.step % 120 == 0 || result.failure || result.fallbacks)
            CE_LOG_DEBUG(
                CE::Logging::Categories::engine,
                "subsystem=events event=simulation-step-starting operation=notification_attempt session={} step={} delta_seconds={} view_revision={} required_entries={} fallbacks={} required_failed={} sample_period=120",
                phase.session, phase.step, phase.delta_seconds, phase.view_revision, result.entered, result.fallbacks, bool(result.failure)
            );
        if (result.failure)
            std::rethrow_exception(result.failure);
    }
    void RuntimeServicesAccess::close(RuntimeServices& services) noexcept {
        auto& state = *services.state_;
        {
            std::lock_guard lock(state.mutex);
            state.closed = true;
            state.bus = nullptr;
        }
        state.maintenance.close();
        state.simulation.close();
    }
    void RuntimeServicesAccess::before_maintenance(const RuntimeServices& services, std::function<void()> callback) {
        services.state_->before_maintenance = std::move(callback);
    }
    void RuntimeServicesAccess::before_step(const RuntimeServices& services, std::function<void()> callback) {
        services.state_->before_step = std::move(callback);
    }
}

namespace CE::Runtime {
    RuntimeServices::RuntimeServices(CE::Events::EventBus& bus)
    : state_(std::make_shared<Detail::RuntimeServicesState>(bus)) {}
    ServiceAttachment RuntimeServices::on_maintenance(std::function<void(const PlatformMaintenance&)> callback) const {
        {
            std::lock_guard lock(state_->mutex);
            state_->require_owner(state_->platform_owner);
        }
        return ServiceAttachment(ViewAttachment(state_->maintenance.attach(*state_->bus, platform_maintenance_event, std::move(callback))));
    }
    ServiceAttachment RuntimeServices::on_step(std::function<void(const SimulationStepStarting&)> callback) const {
        {
            std::lock_guard lock(state_->mutex);
            state_->require_owner(state_->simulation_owner);
        }
        return ServiceAttachment(
            ViewAttachment(state_->simulation.attach(*state_->bus, simulation_step_starting_event, std::move(callback)))
        );
    }
}
