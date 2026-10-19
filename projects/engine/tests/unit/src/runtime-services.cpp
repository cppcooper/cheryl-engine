#include <core/engine/runtime-services.h>
#include "core/engine/runtime-services-internal.h"
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <future>
#include <limits>
#include <stdexcept>
#include <vector>

TEST(runtime_services, required_and_late) {
    using namespace CE::Runtime;
    using Access = Detail::RuntimeServicesAccess;
    CE::Events::EventBus bus;
    auto services = Access::make(bus);
    Access::begin(services, 11, 12);
    EXPECT_THROW(static_cast<void>(services.on_step([](const auto&) {})), CE::Exceptions::failed_operation);
    Access::bind_simulation(services);
    int first = 0;
    int second = 0;
    auto failing = services.on_step([&](const auto&) {
        ++first;
        throw std::runtime_error("partial service action");
    });
    auto later = services.on_step([&](const auto&) { ++second; });
    Access::before_step(services, [] { throw std::bad_alloc{}; });
    EXPECT_THROW(Access::step(services, .1, Simulation::UpdateKind::Fixed, .2, 7), std::runtime_error);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 1);
    bus.notify(simulation_step_starting_event, SimulationStepStarting{});
    EXPECT_EQ(first, 1); // Public facts cannot authorize required operations.
    failing.reset();
    later.reset();
    Access::before_step(services, {});
    ServiceAttachment added;
    int calls = 0;
    bus.register_listener(simulation_step_starting_event, [&](const auto&) {
        if (!added.attached())
            added = services.on_step([&](const auto&) { ++calls; });
    });
    Access::step(services, .1, Simulation::UpdateKind::Fixed, 0, 7);
    EXPECT_EQ(calls, 0);
    Access::step(services, .2, Simulation::UpdateKind::VariableCatchUp, .3, 8);
    EXPECT_EQ(calls, 1);
    Access::close(services);
    EXPECT_FALSE(added.attached());
}

TEST(runtime_services, owner_and_payload) {
    using namespace CE::Runtime;
    using Access = Detail::RuntimeServicesAccess;
    CE::Events::EventBus bus;
    auto services = Access::make(bus);
    Access::begin(services, 11, 12);
    Access::bind_simulation(services);
    std::vector<PlatformMaintenance> maintenance;
    std::vector<SimulationStepStarting> steps;
    auto platform = services.on_maintenance([&](const auto& phase) { maintenance.push_back(phase); });
    auto simulation = services.on_step([&](const auto& phase) { steps.push_back(phase); });
    Access::maintenance(services, MaintenanceReason::Running);
    Access::maintenance(services, MaintenanceReason::FinalCollection);
    EXPECT_EQ(maintenance.size(), 2u);
    EXPECT_EQ(maintenance.back().pass, 2u);
    EXPECT_EQ(maintenance.back().reason, MaintenanceReason::FinalCollection);
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(Access::step(services, nan, Simulation::UpdateKind::Fixed, 0, 7), CE::Exceptions::invalid_args);
    Access::step(services, .1, Simulation::UpdateKind::Fixed, 0, 7);
    Access::step(services, .2, Simulation::UpdateKind::VariableCatchUp, .3, 8);
    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[0].step, 1u);
    EXPECT_EQ(steps[1].session, 12u);
    EXPECT_EQ(steps[1].delta_seconds, .2);
    EXPECT_EQ(steps[1].dropped_seconds, .3);
    EXPECT_EQ(steps[1].view_revision, 8u);
    auto mismatch = std::async(std::launch::async, [&] {
        EXPECT_THROW(static_cast<void>(services.on_step([](const auto&) {})), CE::Exceptions::failed_operation);
    });
    mismatch.get();
    auto retained = services;
    Access::close(services);
    EXPECT_FALSE(platform.attached());
    EXPECT_FALSE(simulation.attached());
    EXPECT_THROW(static_cast<void>(retained.on_step([](const auto&) {})), CE::Exceptions::failed_operation);
}
