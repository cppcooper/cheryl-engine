#pragma once

#include <core/engine/view-state.h>

namespace CE::Runtime::Detail {
    struct ViewSourceAccess {
        CE_API static WindowStateSource window_source(CE::Events::EventBus&);
        CE_API static SimulationViewSource simulation_source(CE::Events::EventBus&);
        CE_API static std::uint64_t identity();
        CE_API static void reserve(WindowStateSource&, std::uint64_t source, std::uint64_t session);
        CE_API static void reserve(SimulationViewSource&, std::uint64_t source, std::uint64_t session);
        CE_API static bool select(WindowStateSource&, WindowState);
        CE_API static bool select(SimulationViewSource&, SimulationView);
        CE_API static void close(WindowStateSource&) noexcept;
        CE_API static void close(SimulationViewSource&) noexcept;
        // Controlled failure before the bus snapshot; required watches must still run.
        CE_API static void before_notify(WindowStateSource&, std::function<void()>);
        CE_API static void before_notify(SimulationViewSource&, std::function<void()>);
    };
}
