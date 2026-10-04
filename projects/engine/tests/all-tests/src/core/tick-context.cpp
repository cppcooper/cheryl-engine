#include <gtest/gtest.h>

#include <core/controls/input-accumulator.h>
#include <core/controls/input-bindings.h>
#include <core/game-framework/tick-context.h>
#include <internals/exceptions.h>

#include <limits>

using namespace std::chrono_literals;

TEST(tick_context, observed_and_simulated_time) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind key{1, 32};
    const CE::Input::ActionId move{1};
    (void)bindings.bind_button(key, move);
    const auto start = bindings.action_snapshot()->observed_at();
    CE::Input::InputAccumulator accumulator(bindings.action_snapshot(), start);
    bindings.on_button(key, true);
    const auto press = bindings.publish_actions(start + 100ms);
    bindings.on_button(key, false);
    const auto release = bindings.publish_actions(start + 150ms);

    const auto input = accumulator.consume(start + 500ms, {press, release});
    const CE::GFramework::TickContext fixed{0.020, input, {}};
    EXPECT_NEAR(fixed.observed_seconds(), 0.500, 1e-9);
    EXPECT_NEAR(input.button(move).down_duration.count(), 0.050, 1e-9);
    EXPECT_NEAR(fixed.button_simulation_seconds(move), 0.002, 1e-9);
    const CE::GFramework::TickContext recovery{0.100, input, {}, CE::GFramework::UpdateKind::VariableCatchUp, 0.400};
    EXPECT_NEAR(recovery.button_simulation_seconds(move), 0.010, 1e-9);

    // A second recovery call has no new polls: released State persists, the tap
    // and its one-shot edge are not replayed to recover missing history.
    const auto next = accumulator.consume(start + 500ms, {});
    const CE::GFramework::TickContext later{0.020, next, {}};
    EXPECT_FALSE(next.button(move).pressed());
    EXPECT_DOUBLE_EQ(later.button_simulation_seconds(move), 0.0);
}

TEST(tick_context, zero_observation_interval) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind key{1, 32};
    const CE::Input::ActionId move{1};
    (void)bindings.bind_button(key, move);
    const auto start = bindings.action_snapshot()->observed_at();
    CE::Input::InputAccumulator accumulator(bindings.action_snapshot(), start);
    bindings.on_button(key, true);
    const auto press = bindings.publish_actions(start + 10ms);
    (void)accumulator.consume(start + 10ms, {press});
    const auto next = accumulator.consume(start + 10ms, {});
    const CE::GFramework::TickContext tick{0.020, next, {}};
    EXPECT_DOUBLE_EQ(tick.observed_seconds(), 0.0);
    EXPECT_FALSE(next.button(move).pressed());
    EXPECT_DOUBLE_EQ(tick.button_simulation_seconds(move), 0.020);
}

TEST(tick_context, input_during_recovery) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind key{1, 32};
    const CE::Input::ActionId move{1};
    (void)bindings.bind_button(key, move);
    const auto start = bindings.action_snapshot()->observed_at();
    CE::Input::InputAccumulator accumulator(bindings.action_snapshot(), start);
    const auto before = accumulator.consume(start + 500ms, {});
    EXPECT_FALSE(before.button(move).held());
    bindings.on_button(key, true);
    const auto press = bindings.publish_actions(start + 501ms);
    const auto after = accumulator.consume(start + 502ms, {press});
    const CE::GFramework::TickContext tick{0.020, after, {}};
    EXPECT_TRUE(after.button(move).pressed());
    EXPECT_NEAR(tick.button_simulation_seconds(move), 0.010, 1e-9);
    const auto persistent = accumulator.consume(start + 502ms, {});
    EXPECT_FALSE(persistent.button(move).pressed());
    EXPECT_TRUE(persistent.button(move).held());
}

TEST(tick_context, invalid_simulation_delta) {
    CE::Input::InputBindings bindings;
    const CE::Input::TickInput input(bindings.action_snapshot(), {});
    const CE::GFramework::TickContext negative{-1.0, input, {}};
    const CE::GFramework::TickContext infinite{std::numeric_limits<double>::infinity(), input, {}};
    EXPECT_THROW((void)negative.button_simulation_seconds(CE::Input::ActionId{1}), CE::Exceptions::invalid_args);
    EXPECT_THROW((void)infinite.button_simulation_seconds(CE::Input::ActionId{1}), CE::Exceptions::invalid_args);
}

TEST(tick_context, recovery_input_consumption) {
    CE::Input::InputBindings bindings;
    const CE::Input::DeviceBind key{1, 32};
    const CE::Input::DeviceBind wheel{2, 1};
    const CE::Input::ActionId move{1};
    const CE::Input::ActionId scroll{2};
    (void)bindings.bind_button(key, move);
    CE::Input::AxisOptions relative;
    relative.kind = CE::Input::AxisKind::Relative;
    (void)bindings.bind_axis(wheel, scroll, relative);
    const auto start = bindings.action_snapshot()->observed_at();
    CE::Input::InputAccumulator accumulator(bindings.action_snapshot(), start);
    bindings.on_button(key, true);
    bindings.on_delta(wheel, 3.0f);
    auto poll = std::make_shared<CE::Input::PollSnapshot>();
    poll->state = bindings.publish_actions(start + 100ms);
    poll->records.push_back(
        {1, start + 100ms, 1, CE::Input::DeviceKind::Keyboard, CE::Input::ButtonEvent{32, CE::Input::ButtonPhase::Press}}
    );
    poll->records.push_back({2, start + 100ms, 1, CE::Input::DeviceKind::Keyboard, CE::Input::TextEvent{U'a'}});

    CE::GFramework::SimulationTimingOptions options;
    options.mode = CE::GFramework::SimulationMode::Fixed;
    options.fixed_step = 20ms;
    options.max_fixed_updates = 2;
    options.recovery = CE::GFramework::LagRecovery::VariableCatchUp;
    options.fixed_updates_before_recovery = 2;
    CE::GFramework::SimulationScheduler scheduler(options, start);
    const auto batch = scheduler.advance(start + 500ms);
    ASSERT_EQ(batch.steps.size(), 3u);

    const auto first = accumulator.consume_polls(start + 500ms, {poll});
    EXPECT_TRUE(first.button(move).pressed());
    EXPECT_FLOAT_EQ(first.axis(scroll).delta(), 3.0f);
    ASSERT_EQ(first.records().size(), 2u);
    EXPECT_TRUE(first.records()[1].is_text());
    // The next fixed step and larger recovery receive held State, not the first
    // call's records or relative motion, even at the same observation timestamp.
    for (std::size_t i = 1; i < batch.steps.size(); ++i) {
        const auto next = accumulator.consume_polls(start + 500ms, {});
        EXPECT_TRUE(next.button(move).held());
        EXPECT_FALSE(next.button(move).pressed());
        EXPECT_EQ(next.button(move).press_count, 0u);
        EXPECT_FLOAT_EQ(next.axis(scroll).delta(), 0.0f);
        EXPECT_TRUE(next.records().empty());
    }
}
