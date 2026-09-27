#include <gtest/gtest.h>
#include <templates/observed-variables.h>
#include <math/time.h>
#include <atomic>
#include <thread>

TEST(templates_ovar, notifies_both_observers_and_unblocks_a_waiter_on_change) {
    std::atomic<int> first_observed{-1};
    std::atomic<int> second_observed{-1};
    ObservedVariable<int, 2> value(0, {
        [&](const int& changed) { first_observed.store(changed); },
        [&](const int& changed) { second_observed.store(changed); }
    });

    // Start a waiter before changing the value. The current API exposes no
    // readiness signal, so this delay is a best-effort scheduling allowance.
    std::thread wait_thread([&value]() {
        value.wait_until_change();
    });
    std::this_thread::sleep_for(Seconds{2});

    // One change should release the waiter and pass the new value to each
    // observer, rather than just calling the observers with the old value.
    value.set(10);
    wait_thread.join();
    EXPECT_EQ(value.get(), 10);
    EXPECT_EQ(first_observed.load(), 10);
    EXPECT_EQ(second_observed.load(), 10);
}
