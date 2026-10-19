#include <core/game-framework/runtime-pressure-internal.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
    using Source = CE::Runtime::Detail::RuntimePressureSource;
    using Change = CE::Runtime::Detail::RuntimePressureChanged;
    using Conditions = CE::Runtime::Detail::RuntimePressureConditions;
    using Clock = CE::Runtime::Simulation::Clock;
}

TEST(runtime_pressure, transitions) {
    CE::Events::EventBus events;
    Source source;
    std::vector<Change> changes;
    auto watch = source.watch(events, [&](const Change& change) { changes.push_back(change); });
    EXPECT_NE(watch.seed.source, 0u);
    EXPECT_EQ(watch.seed.revision, 0u);
    EXPECT_EQ(watch.seed.conditions, Conditions{});
    source.bind_owner();
    source.publish(events, false, 0, Clock::now());
    EXPECT_TRUE(changes.empty());
    source.stage_lag(true, 2, 100);
    source.publish(events, true, 3, Clock::now());
    source.publish(events, true, 4, Clock::now());
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].previous, Conditions{});
    EXPECT_EQ(changes[0].current.conditions, (Conditions{true, true}));
    EXPECT_EQ(changes[0].current.dropped_batches, 2u);
    EXPECT_EQ(changes[0].current.dropped_nanoseconds, 100u);
    EXPECT_EQ(changes[0].current.backpressure_observations, 3u);
    source.stage_lag(false, 2, 100);
    source.publish(events, true, 5, Clock::now());
    source.publish(events, false, 5, Clock::now());
    ASSERT_EQ(changes.size(), 3u);
    EXPECT_EQ(changes[1].current.conditions, (Conditions{false, true}));
    EXPECT_EQ(changes[2].current.conditions, Conditions{});
    EXPECT_EQ(changes[2].current.revision, 3u);
    EXPECT_EQ(changes[2].current.source, watch.seed.source);
    EXPECT_LE(changes[0].current.observed_at, changes[2].current.observed_at);
    EXPECT_TRUE(events.unregister_and_wait(watch.registration));
}

TEST(runtime_pressure, owner_staging) {
    CE::Events::EventBus events;
    Source source;
    source.bind_owner();
    const auto platform_owner = std::this_thread::get_id();
    std::vector<Change> changes;
    const auto watch = source.watch(events, [&](const Change& change) {
        EXPECT_EQ(std::this_thread::get_id(), platform_owner);
        changes.push_back(change);
    });
    std::thread simulation([&] { source.stage_lag(true, 7, 900); });
    simulation.join();
    EXPECT_TRUE(changes.empty());
    source.publish(events, false, 0, Clock::now());
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_TRUE(changes.front().current.conditions.simulation_lag);
    EXPECT_EQ(changes.front().current.dropped_batches, 7u);
    EXPECT_TRUE(events.unregister_and_wait(watch.registration));
}

TEST(runtime_pressure, late_watch) {
    CE::Events::EventBus events;
    Source source;
    source.bind_owner();
    source.stage_lag(true, 2, 100);
    source.publish(events, false, 0, Clock::now());
    std::vector<Change> changes;
    const auto watch = source.watch(events, [&](const Change& change) { changes.push_back(change); });
    EXPECT_TRUE(changes.empty());
    EXPECT_EQ(watch.seed.revision, 1u);
    EXPECT_TRUE(watch.seed.conditions.simulation_lag);
    source.publish(events, false, 0, Clock::now());
    EXPECT_TRUE(changes.empty());
    source.stage_lag(false, 2, 100);
    source.publish(events, false, 0, Clock::now());
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes.front().current.revision, 2u);
    EXPECT_TRUE(events.unregister_and_wait(watch.registration));
}

TEST(runtime_pressure, reentry) {
    CE::Events::EventBus events;
    Source source;
    source.bind_owner();
    const auto first = source.watch(events, [&](const Change& change) {
        source.stage_lag(!change.current.conditions.simulation_lag, 2, 100);
        source.publish(events, false, 0, Clock::now());
        throw std::runtime_error("optional pressure observer");
    });
    std::vector<std::uint64_t> revisions;
    const auto later = source.watch(events, [&](const Change& change) { revisions.push_back(change.current.revision); });
    source.stage_lag(true, 2, 100);
    source.publish(events, false, 0, Clock::now());
    EXPECT_EQ(revisions, (std::vector<std::uint64_t>{1}));
    source.publish(events, false, 0, Clock::now());
    EXPECT_EQ(revisions, (std::vector<std::uint64_t>{1, 2}));
    EXPECT_TRUE(events.unregister_and_wait(first.registration));
    EXPECT_TRUE(events.unregister_and_wait(later.registration));
}

TEST(runtime_pressure, closure) {
    CE::Events::EventBus events;
    Source source;
    source.bind_owner();
    int calls = 0;
    const auto watch = source.watch(events, [&](const Change&) { ++calls; });
    source.publish(events, true, 5, Clock::now());
    source.close();
    source.close();
    source.stage_lag(false, 0, 0);
    source.publish(events, false, 5, Clock::now());
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(source.current().closed);
    EXPECT_TRUE(source.current().conditions.input_backpressure);
    EXPECT_FALSE(events.diagnostics().closed);
    EXPECT_TRUE(events.unregister_and_wait(watch.registration));
    const auto late = source.watch(events, [&](const Change&) { ++calls; });
    EXPECT_TRUE(late.seed.closed);
    EXPECT_EQ(late.registration.id(), 0u);
}

TEST(runtime_pressure, source_identity) {
    CE::Events::EventBus events;
    Source first;
    Source second;
    first.bind_owner();
    second.bind_owner();
    int calls = 0;
    const auto watch = first.watch(events, [&](const Change&) { ++calls; });
    EXPECT_NE(first.current().source, second.current().source);
    second.publish(events, true, 1, Clock::now());
    EXPECT_EQ(calls, 0);
    first.publish(events, true, 1, Clock::now());
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(events.unregister_and_wait(watch.registration));
}

TEST(runtime_pressure, missed_notification) {
    CE::Events::EventBus events;
    Source source;
    source.bind_owner();
    events.close();
    source.stage_lag(true, 2, 100);
    EXPECT_NO_THROW(source.publish(events, false, 0, Clock::now()));
    const auto current = source.current();
    EXPECT_EQ(current.revision, 1u);
    EXPECT_TRUE(current.conditions.simulation_lag);
    EXPECT_FALSE(current.closed);
}

TEST(runtime_pressure, watch_failure) {
    CE::Events::EventBus events;
    Source source;
    source.bind_owner();
    struct Target {
        Source& source;
        bool& released;

        ~Target() {
            EXPECT_FALSE(source.current().closed); // Callback release must occur outside the source mutex.
            released = true;
        }
    };
    bool released = false;
    auto target = std::shared_ptr<Target>(new Target{source, released});
    CE::Events::EventChannel<Change>::Callback callback = [target](const Change&) {};
    target.reset();
    events.close();
    EXPECT_THROW(static_cast<void>(source.watch(events, std::move(callback))), CE::Exceptions::failed_operation);
    EXPECT_TRUE(released);
    EXPECT_EQ(source.current().revision, 0u);
}
