#include <core/game-framework/runtime-pressure-internal.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <thread>

namespace {
    using State = CE::Runtime::Detail::RuntimePressureState;
    using Conditions = CE::Runtime::Detail::RuntimePressureConditions;
    using Clock = CE::Runtime::Simulation::Clock;
}

TEST(runtime_pressure, transitions) {
    State state;
    const auto seed = state.current();
    EXPECT_NE(seed.source, 0u);
    EXPECT_EQ(seed.revision, 0u);
    EXPECT_EQ(seed.conditions, Conditions{});
    state.bind_owner();
    EXPECT_FALSE(state.take_change(false, 0, Clock::now()));
    state.stage_lag(true, 2, 100);
    const auto first = state.take_change(true, 3, Clock::now());
    ASSERT_TRUE(first);
    EXPECT_EQ(first->previous, Conditions{});
    EXPECT_EQ(first->current.conditions, (Conditions{true, true}));
    EXPECT_EQ(first->current.dropped_batches, 2u);
    EXPECT_EQ(first->current.dropped_nanoseconds, 100u);
    EXPECT_EQ(first->current.backpressure_observations, 3u);
    EXPECT_FALSE(state.take_change(true, 4, Clock::now()));
    state.stage_lag(false, 2, 100);
    const auto second = state.take_change(true, 5, Clock::now());
    const auto third = state.take_change(false, 5, Clock::now());
    ASSERT_TRUE(second);
    ASSERT_TRUE(third);
    EXPECT_EQ(second->current.conditions, (Conditions{false, true}));
    EXPECT_EQ(third->current.conditions, Conditions{});
    EXPECT_EQ(third->current.revision, 3u);
    EXPECT_EQ(third->current.source, seed.source);
    EXPECT_LE(first->current.observed_at, third->current.observed_at);
}

TEST(runtime_pressure, owner_staging) {
    State state;
    state.bind_owner();
    std::thread simulation([&] { state.stage_lag(true, 7, 900); });
    simulation.join();
    EXPECT_EQ(state.current().revision, 0u);
    EXPECT_FALSE(state.current().conditions.simulation_lag);
    const auto change = state.take_change(false, 0, Clock::now());
    ASSERT_TRUE(change);
    EXPECT_TRUE(change->current.conditions.simulation_lag);
    EXPECT_EQ(change->current.dropped_batches, 7u);
    EXPECT_EQ(change->current.dropped_nanoseconds, 900u);
}

TEST(runtime_pressure, closure) {
    State state;
    state.bind_owner();
    ASSERT_TRUE(state.take_change(true, 5, Clock::now()));
    state.close();
    state.close();
    state.stage_lag(false, 0, 0);
    EXPECT_FALSE(state.take_change(false, 5, Clock::now()));
    EXPECT_TRUE(state.current().closed);
    EXPECT_TRUE(state.current().conditions.input_backpressure);
    EXPECT_EQ(state.current().revision, 1u);
}

TEST(runtime_pressure, source_identity) {
    State first;
    State second;
    EXPECT_NE(first.current().source, second.current().source);
    EXPECT_NE(first.current().source, 0u);
    EXPECT_NE(second.current().source, 0u);
}

TEST(runtime_pressure, owner_validation) {
    State state;
    EXPECT_EQ(state.owner_snapshot().revision, 0u); // Synchronized pre-run observation.
    state.bind_owner();
    bool rejected = false;
    std::thread other([&] {
        try {
            static_cast<void>(state.owner_snapshot());
        } catch (const CE::Exceptions::failed_operation&) {
            rejected = true;
        }
        EXPECT_FALSE(state.take_change(true, 1, Clock::now()));
    });
    other.join();
    EXPECT_TRUE(rejected);
    EXPECT_EQ(state.current().revision, 0u);
}
