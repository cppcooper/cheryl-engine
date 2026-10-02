#include <atomic>
#include <gtest/gtest.h>
#include <templates/observed-variables.h>
#include <thread>

TEST(templates_ovar, observers_and_waiters) {
    std::atomic<int> first_observed{-1};
    std::atomic<int> second_observed{-1};
    ObservedVariable<int, 2> value(
        0, {[&](const int& changed) { first_observed.store(changed); }, [&](const int& changed) { second_observed.store(changed); }});

    // Capture the revision before starting the waiter. It will return even if
    // set() happens before the waiter is scheduled on its own thread.
    const auto revision = value.revision();
    std::thread wait_thread([&value, revision] { (void)value.wait_for_change(revision); });

    // One change should release the waiter and pass the new value to each
    // observer, rather than just calling the observers with the old value.
    value.set(10);
    wait_thread.join();
    EXPECT_EQ(value.get(), 10);
    EXPECT_EQ(value.revision(), revision + 1);
    EXPECT_EQ(first_observed.load(), 10);
    EXPECT_EQ(second_observed.load(), 10);

    // Repeating an equal value calls observers on this setter thread without a new revision.
    value.set(10);
    EXPECT_EQ(value.revision(), revision + 1);
}
