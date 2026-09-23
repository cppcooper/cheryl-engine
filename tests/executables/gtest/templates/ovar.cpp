#include <gtest/gtest.h>
#include <templates/observed-variables.h>
#include <math/time.h>
#include <thread>

std::atomic_bool cb1{false};
std::atomic_bool cb2{false};

TEST(templates_ovar, simple) {
    // Register two callbacks to observe a change from the initial value.
    ObservedVariable<int,2> var(0,{
        [](const int&){cb1.store(true);},
        [](const int&){cb2.store(true);}
    });

    // Start a waiter before changing the value. Joinable only means the thread has
    // not been joined yet; it does not establish that the waiter is blocked.
    std::thread wait_thread([&var]() {
        var.wait_until_change();
    });
    std::this_thread::sleep_for(Seconds{2});
    ASSERT_TRUE(wait_thread.joinable());

    // Set a new value, join the waiter, then confirm both callbacks ran.
    var.set(10);
    wait_thread.join();
    ASSERT_TRUE(cb1.load());
    ASSERT_TRUE(cb2.load());
}
