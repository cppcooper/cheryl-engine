#include <ui/rmlui/session.h>
#include <core/engine/view-state.h>
#include "core/engine/view-state-internal.h"
#include "core/engine/runtime-services-internal.h"
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/SystemInterface.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>
#include <future>

namespace {
    class ViewInput final : public CE::Input::System {
        CE::Input::Actions::Bindings bindings_;
    public:
        void initialize(CE::Platform::Window&) override {}
        void deinitialize() override {}
        void poll() override {}
        CE::Input::Actions::Bindings& bindings() override { return bindings_; }
        CE::Input::DeviceId keyboard_id() const override { return 1; }
        CE::Input::DeviceId mouse_id() const override { return 2; }
        CE::Input::DeviceId gamepad_id() const override { return 3; }
        bool supports(CE::Input::Capture::Channel) const override { return true; }
    };
}

TEST(ui_rmlui_view, automatic_and_manual) {
    using Access = CE::Runtime::Detail::ViewSourceAccess;
    CE::Events::EventBus bus;
    auto source = Access::simulation_source(bus);
    Access::reserve(source, Access::identity(), Access::identity());
    CE::Runtime::SimulationView selected;
    selected.logical_size = {100, 80};
    selected.framebuffer_size = {200, 160};
    Access::select(source, selected);
    ViewInput input;
    CE::UI::RmlUi::Session session(input, 7);
    bool manual_override = false;
    bus.register_listener(CE::Runtime::simulation_view_changed_event, [&](const auto&) {
        if (manual_override)
            session.set_view({30, 20}, {60, 40});
    });
    session.follow_view(source);
    EXPECT_EQ(session.context().GetDimensions(), (Rml::Vector2i{100, 80}));
    EXPECT_THROW(session.set_view({-1, 80}, {200, 160}), CE::Exceptions::invalid_args);
    selected.logical_size = {120, 90};
    Access::select(source, selected);
    EXPECT_EQ(session.context().GetDimensions(), (Rml::Vector2i{120, 90}));
    manual_override = true;
    selected.logical_size = {150, 100};
    Access::select(source, selected);
    EXPECT_EQ(session.context().GetDimensions(), (Rml::Vector2i{30, 20}));
    manual_override = false;
    selected.logical_size = {160, 110};
    Access::select(source, selected);
    EXPECT_EQ(session.context().GetDimensions(), (Rml::Vector2i{30, 20}));
    session.follow_view(source); // Late attachment seeds the current selection.
    EXPECT_EQ(session.context().GetDimensions(), (Rml::Vector2i{160, 110}));
    auto mismatch = std::async(std::launch::async, [&] { EXPECT_THROW(session.follow_view(source), CE::Exceptions::failed_operation); });
    mismatch.get();
    session.close_after_quiescence();
    selected.logical_size = {170, 120};
    EXPECT_NO_THROW(Access::select(source, selected));
    Access::close(source);
}

TEST(ui_rmlui_view, uninitialized) {
    using Access = CE::Runtime::Detail::ViewSourceAccess;
    CE::Events::EventBus bus;
    auto source = Access::simulation_source(bus);
    ViewInput input;
    CE::UI::RmlUi::Session session(input, 7);
    session.set_view({30, 20}, {60, 40});
    EXPECT_THROW(session.follow_view(source), CE::Exceptions::failed_operation);
    EXPECT_EQ(session.context().GetDimensions(), (Rml::Vector2i{30, 20}));
    Access::close(source);
}

TEST(ui_rmlui_time, automatic_and_manual) {
    using Access = CE::Runtime::Detail::RuntimeServicesAccess;
    CE::Events::EventBus bus;
    auto services = Access::make(bus);
    Access::begin(services, 11, 12);
    Access::bind_simulation(services);
    Access::step(services, .25, CE::Runtime::Simulation::UpdateKind::Fixed, 0, 1);
    ViewInput input;
    CE::UI::RmlUi::Session session(input, 7);
    session.update_time(.5);
    const auto elapsed = [] { return Rml::GetSystemInterface()->GetElapsedTime(); };
    EXPECT_DOUBLE_EQ(elapsed(), .5);
    bus.register_listener(CE::Runtime::simulation_step_starting_event, [&](const auto& phase) {
        if (phase.step == 2)
            session.follow_time(services);
    });
    Access::step(services, .25, CE::Runtime::Simulation::UpdateKind::Fixed, 0, 1);
    EXPECT_DOUBLE_EQ(elapsed(), .5);
    EXPECT_THROW(session.update_time(.25), CE::Exceptions::failed_operation);
    Access::step(services, .25, CE::Runtime::Simulation::UpdateKind::VariableCatchUp, .1, 1);
    EXPECT_DOUBLE_EQ(elapsed(), .75);
    session.detach_time();
    session.update_time(.1);
    EXPECT_DOUBLE_EQ(elapsed(), .85);
    Access::step(services, .25, CE::Runtime::Simulation::UpdateKind::Fixed, 0, 1);
    EXPECT_DOUBLE_EQ(elapsed(), .85);
    session.close_after_quiescence();
    Access::step(services, .25, CE::Runtime::Simulation::UpdateKind::Fixed, 0, 1);
    Access::close(services);
}
