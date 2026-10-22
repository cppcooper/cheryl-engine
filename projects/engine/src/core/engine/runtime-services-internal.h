#pragma once
#include <core/engine/runtime-services.h>

namespace CE::Runtime::Detail {
    struct RuntimeServicesAccess {
        CE_API static RuntimeServices make(CE::Events::EventBus&);
        CE_API static void begin(RuntimeServices&, std::uint64_t context, std::uint64_t session);
        CE_API static void bind_simulation(RuntimeServices&);
        CE_API static void maintenance(RuntimeServices&, MaintenanceReason);
        CE_API static void step(RuntimeServices&, double delta, Simulation::UpdateKind, double dropped, std::uint64_t view_revision);
        CE_API static void close(RuntimeServices&) noexcept;
        CE_API static void before_maintenance(const RuntimeServices&, std::function<void()>);
        CE_API static void before_step(const RuntimeServices&, std::function<void()>);
    };
}
