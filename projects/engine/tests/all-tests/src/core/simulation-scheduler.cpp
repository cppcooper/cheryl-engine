#include <gtest/gtest.h>

#include <core/game-framework/simulation-scheduler.h>
#include <internals/exceptions.h>

using namespace std::chrono_literals;

namespace {
    using Clock = CE::GFramework::SimulationClock;
    using CE::GFramework::LagRecovery;
    using CE::GFramework::SimulationMode;
    using CE::GFramework::SimulationScheduler;
    using CE::GFramework::SimulationTimingOptions;
    using CE::GFramework::UpdateKind;

    SimulationTimingOptions fixed_timing() {
        SimulationTimingOptions options;
        options.mode = SimulationMode::Fixed;
        options.fixed_step = 20ms;
        return options;
    }
}

TEST(simulation_scheduler, variable_elapsed_time) {
    SimulationTimingOptions options;
    options.variable_interval = 20ms;
    SimulationScheduler scheduler(options, Clock::time_point{});
    EXPECT_TRUE(scheduler.advance(Clock::time_point{} + 5ms).steps.empty());
    EXPECT_EQ(scheduler.next_update_at(), Clock::time_point{} + 20ms);

    const auto batch = scheduler.advance(Clock::time_point{} + 23ms);
    ASSERT_EQ(batch.steps.size(), 1u);
    EXPECT_EQ(batch.steps[0].kind, UpdateKind::Variable);
    EXPECT_EQ(batch.steps[0].delta, 23ms);
    EXPECT_EQ(batch.observed_elapsed, 18ms);
    EXPECT_EQ(batch.dropped, 0ms);
    EXPECT_EQ(scheduler.next_update_at(), Clock::time_point{} + 43ms);
}

TEST(simulation_scheduler, bounded_stall_recovery) {
    SimulationScheduler scheduler(fixed_timing(), Clock::time_point{});
    const auto batch = scheduler.advance(Clock::time_point{} + 500ms);
    ASSERT_EQ(batch.steps.size(), 1u);
    EXPECT_EQ(batch.steps[0].kind, UpdateKind::Fixed);
    EXPECT_EQ(batch.steps[0].delta, 20ms);
    EXPECT_EQ(batch.dropped, 480ms);
    EXPECT_EQ(scheduler.next_update_at(), Clock::time_point{} + 520ms);
}

TEST(simulation_scheduler, fixed_step_remainder) {
    auto options = fixed_timing();
    options.fixed_step = 30ms;
    options.max_fixed_updates = 2;
    SimulationScheduler scheduler(options, Clock::time_point{});
    const auto batch = scheduler.advance(Clock::time_point{} + 500ms);
    ASSERT_EQ(batch.steps.size(), 2u);
    EXPECT_EQ(batch.steps[0].delta, 30ms);
    EXPECT_EQ(batch.steps[1].delta, 30ms);
    EXPECT_EQ(batch.dropped, 420ms);
    EXPECT_EQ(scheduler.next_update_at(), Clock::time_point{} + 510ms);

    EXPECT_TRUE(scheduler.advance(Clock::time_point{} + 509ms).steps.empty());
    const auto next = scheduler.advance(Clock::time_point{} + 510ms);
    ASSERT_EQ(next.steps.size(), 1u);
    EXPECT_EQ(next.steps[0].delta, 30ms);
    EXPECT_EQ(next.dropped, 0ms);
}

TEST(simulation_scheduler, capped_variable_recovery) {
    auto options = fixed_timing();
    options.recovery = LagRecovery::VariableCatchUp;
    SimulationScheduler scheduler(options, Clock::time_point{});
    const auto batch = scheduler.advance(Clock::time_point{} + 500ms);
    ASSERT_EQ(batch.steps.size(), 1u);
    EXPECT_EQ(batch.steps[0].kind, UpdateKind::VariableCatchUp);
    EXPECT_EQ(batch.steps[0].delta, 100ms);
    EXPECT_EQ(batch.dropped, 400ms);
    EXPECT_EQ(scheduler.next_update_at(), Clock::time_point{} + 520ms);
}

TEST(simulation_scheduler, hybrid_fixed_prefix) {
    auto options = fixed_timing();
    options.recovery = LagRecovery::VariableCatchUp;
    options.max_fixed_updates = 3;
    options.fixed_updates_before_recovery = 2;
    SimulationScheduler scheduler(options, Clock::time_point{});
    const auto batch = scheduler.advance(Clock::time_point{} + 500ms);
    ASSERT_EQ(batch.steps.size(), 3u);
    EXPECT_EQ(batch.steps[0].kind, UpdateKind::Fixed);
    EXPECT_EQ(batch.steps[1].kind, UpdateKind::Fixed);
    EXPECT_EQ(batch.steps[2].kind, UpdateKind::VariableCatchUp);
    EXPECT_EQ(batch.steps[2].delta, 100ms);
    EXPECT_EQ(batch.dropped, 360ms);
}

TEST(simulation_scheduler, uncapped_recovery) {
    auto options = fixed_timing();
    options.recovery = LagRecovery::VariableCatchUp;
    options.recovery_cap = 0ms;
    options.fixed_updates_before_recovery = 1;
    SimulationScheduler scheduler(options, Clock::time_point{});
    const auto batch = scheduler.advance(Clock::time_point{} + 500ms);
    ASSERT_EQ(batch.steps.size(), 2u);
    EXPECT_EQ(batch.steps[0].delta, 20ms);
    EXPECT_EQ(batch.steps[1].delta, 480ms);
    EXPECT_EQ(batch.dropped, 0ms);
}

TEST(simulation_scheduler, normal_fixed_steps) {
    auto options = fixed_timing();
    options.recovery = LagRecovery::VariableCatchUp;
    options.max_fixed_updates = 2;
    SimulationScheduler scheduler(options, Clock::time_point{});
    const auto batch = scheduler.advance(Clock::time_point{} + 40ms);
    ASSERT_EQ(batch.steps.size(), 2u);
    EXPECT_EQ(batch.steps[0].kind, UpdateKind::Fixed);
    EXPECT_EQ(batch.steps[1].kind, UpdateKind::Fixed);
    EXPECT_EQ(batch.dropped, 0ms);
}

TEST(simulation_scheduler, expensive_update_backlog) {
    SimulationScheduler scheduler(fixed_timing(), Clock::time_point{});
    const auto first = scheduler.advance(Clock::time_point{} + 20ms);
    ASSERT_EQ(first.steps.size(), 1u);
    // Pretend update() used 75 ms. Its cost never grows the already selected batch.
    const auto second = scheduler.advance(Clock::time_point{} + 95ms);
    ASSERT_EQ(second.steps.size(), 1u);
    EXPECT_EQ(second.steps[0].delta, 20ms);
    EXPECT_EQ(second.dropped, 40ms);
    EXPECT_EQ(scheduler.next_update_at(), Clock::time_point{} + 100ms);
}

TEST(simulation_scheduler, invalid_timing_inputs) {
    auto options = fixed_timing();
    options.fixed_step = 0ms;
    EXPECT_THROW((void)SimulationScheduler(options), CE::Exceptions::invalid_args);
    options = fixed_timing();
    options.max_fixed_updates = 0;
    EXPECT_THROW((void)SimulationScheduler(options), CE::Exceptions::invalid_args);
    options = fixed_timing();
    options.fixed_updates_before_recovery = 2;
    EXPECT_THROW((void)SimulationScheduler(options), CE::Exceptions::invalid_args);
    options = fixed_timing();
    options.recovery_cap = -1ms;
    EXPECT_THROW((void)SimulationScheduler(options), CE::Exceptions::invalid_args);
    options = fixed_timing();
    options.variable_interval = -1ms;
    EXPECT_THROW((void)SimulationScheduler(options), CE::Exceptions::invalid_args);
    SimulationScheduler scheduler(fixed_timing(), Clock::time_point{});
    EXPECT_THROW((void)scheduler.advance(Clock::time_point{} - 1ms), CE::Exceptions::invalid_args);
}

TEST(simulation_scheduler, clock_limits) {
    SimulationScheduler near_end(fixed_timing(), Clock::time_point::max() - 1ms);
    EXPECT_EQ(near_end.next_update_at(), Clock::time_point::max());
    SimulationScheduler wide_span(fixed_timing(), Clock::time_point::min());
    EXPECT_THROW((void)wide_span.advance(Clock::time_point::max()), CE::Exceptions::invalid_args);
}

TEST(simulation_scheduler, unpaced_variable_updates) {
    SimulationTimingOptions options;
    options.variable_interval = 0ms;
    options.fixed_step = 100ms;
    SimulationScheduler scheduler(options, Clock::time_point{});
    EXPECT_EQ(scheduler.next_update_at(), Clock::time_point{});
    const auto batch = scheduler.advance(Clock::time_point{} + 3ms);
    ASSERT_EQ(batch.steps.size(), 1u);
    EXPECT_EQ(batch.steps[0].delta, 3ms);
    EXPECT_EQ(batch.steps[0].kind, UpdateKind::Variable);
}

TEST(simulation_scheduler, independent_fixed_pacing) {
    auto options = fixed_timing();
    options.variable_interval = 500ms;
    SimulationScheduler scheduler(options, Clock::time_point{});
    EXPECT_EQ(scheduler.next_update_at(), Clock::time_point{} + 20ms);
    const auto batch = scheduler.advance(Clock::time_point{} + 20ms);
    ASSERT_EQ(batch.steps.size(), 1u);
    EXPECT_EQ(batch.steps[0].delta, 20ms);
}
