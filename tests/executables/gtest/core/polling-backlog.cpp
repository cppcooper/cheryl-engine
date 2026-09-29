#include <gtest/gtest.h>

#include <core/controls/input-bindings.h>
#include <core/controls/polling-backlog.h>
#include <internals/exceptions.h>

using namespace std::chrono_literals;

TEST(polling_backlog, lockstep_allows_one_completed_poll_until_consumption) {
    CE::Input::InputBindings bindings;
    CE::Input::PollingBacklog backlog;
    const auto now = bindings.action_snapshot()->observed_at();
    const auto poll = bindings.publish_actions(now);
    backlog.complete(poll, now);
    EXPECT_EQ(backlog.completed_polls(), 1u);
    EXPECT_FALSE(backlog.can_poll());
    EXPECT_EQ(backlog.next_poll_at(), CE::Input::InputClock::time_point::max());

    const auto batch = backlog.consume();
    ASSERT_EQ(batch.size(), 1u);
    EXPECT_EQ(batch[0], poll);
    EXPECT_EQ(backlog.completed_polls(), 0u);
    EXPECT_TRUE(backlog.can_poll());
    EXPECT_FALSE(backlog.poll_due(now));
    EXPECT_TRUE(backlog.poll_due(now + 1ms));
}

TEST(polling_backlog, finite_capacity_counts_unchanged_polls_and_hands_off_the_whole_batch) {
    CE::Input::InputBindings bindings;
    CE::Input::PollingBacklog backlog({CE::Input::PollingPolicy::Finite, 3, 0ms});
    const auto now = bindings.action_snapshot()->observed_at();
    for (int i = 0; i < 3; ++i) {
        const auto poll = bindings.publish_actions(now);
        ASSERT_FALSE(poll->has_changes());
        backlog.complete(poll, now);
    }
    EXPECT_FALSE(backlog.can_poll());
    EXPECT_EQ(backlog.completed_polls(), 3u);
    const auto batch = backlog.consume();
    ASSERT_EQ(batch.size(), 3u);
    EXPECT_LT(batch[0]->poll(), batch[1]->poll());
    EXPECT_LT(batch[1]->poll(), batch[2]->poll());
    EXPECT_TRUE(backlog.can_poll());
    EXPECT_TRUE(backlog.consume().empty());
}

TEST(polling_backlog, spacing_survives_consumption_and_is_measured_after_poll_completion) {
    CE::Input::InputBindings bindings;
    CE::Input::PollingBacklog backlog({CE::Input::PollingPolicy::Finite, 2, 5ms});
    const auto now = bindings.action_snapshot()->observed_at();
    backlog.complete(bindings.publish_actions(now + 2ms), now + 3ms);
    EXPECT_FALSE(backlog.poll_due(now + 7ms));
    (void)backlog.consume();
    EXPECT_EQ(backlog.next_poll_at(), now + 8ms);
    EXPECT_FALSE(backlog.poll_due(now + 7ms));
    EXPECT_TRUE(backlog.poll_due(now + 8ms));
    backlog.complete(bindings.publish_actions(now + 8ms), now + 8ms);
}

TEST(polling_backlog, unlimited_keeps_each_completed_poll_without_a_capacity_limit) {
    CE::Input::InputBindings bindings;
    CE::Input::PollingBacklog backlog({CE::Input::PollingPolicy::Unlimited, 0, 0ms});
    const auto now = bindings.action_snapshot()->observed_at();
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(backlog.poll_due(now));
        backlog.complete(bindings.publish_actions(now), now);
    }
    EXPECT_EQ(backlog.consume().size(), 100u);
}

TEST(polling_backlog, invalid_capacity_delay_and_duplicate_observations_are_rejected) {
    const CE::Input::PollingOptions empty{CE::Input::PollingPolicy::Finite, 0, 0ms};
    const CE::Input::PollingOptions negative{CE::Input::PollingPolicy::Finite, 1, -1ms};
    EXPECT_THROW((void)CE::Input::PollingBacklog{empty}, CE::Exceptions::invalid_args);
    EXPECT_THROW((void)CE::Input::PollingBacklog{negative}, CE::Exceptions::invalid_args);
    CE::Input::InputBindings bindings;
    CE::Input::PollingBacklog backlog({CE::Input::PollingPolicy::Unlimited, 0, 0ms});
    const auto now = bindings.action_snapshot()->observed_at();
    const auto poll = bindings.publish_actions(now);
    backlog.complete(poll, now);
    (void)backlog.consume();
    EXPECT_THROW(backlog.complete(poll, now), CE::Exceptions::invalid_args);
}
