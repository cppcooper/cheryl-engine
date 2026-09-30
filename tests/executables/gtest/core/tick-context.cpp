#include <gtest/gtest.h>

#include <core/controls/input-accumulator.h>
#include <core/controls/input-bindings.h>
#include <core/game-framework/tick-context.h>
#include <internals/exceptions.h>

#include <limits>

using namespace std::chrono_literals;

TEST(tick_context, a_short_tap_in_a_half_second_stall_keeps_observation_and_simulation_time_separate) {
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

TEST(tick_context, a_zero_observation_interval_uses_current_held_state) {
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

TEST(tick_context, input_arriving_between_recovery_calls_is_consumed_only_by_the_later_update) {
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

TEST(tick_context, manually_supplied_invalid_simulation_deltas_are_rejected) {
    CE::Input::InputBindings bindings;
    const CE::Input::TickInput input(bindings.action_snapshot(), {});
    const CE::GFramework::TickContext negative{-1.0, input, {}};
    const CE::GFramework::TickContext infinite{std::numeric_limits<double>::infinity(), input, {}};
    EXPECT_THROW((void)negative.button_simulation_seconds(CE::Input::ActionId{1}), CE::Exceptions::invalid_args);
    EXPECT_THROW((void)infinite.button_simulation_seconds(CE::Input::ActionId{1}), CE::Exceptions::invalid_args);
}
