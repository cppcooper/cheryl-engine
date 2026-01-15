#include <gtest/gtest.h>
#include <templates/observed-variables.h>
#include <math/time.h>
#include <thread>

std::atomic_bool cb1{false};
std::atomic_bool cb2{false};

TEST(templates_ovar, simple) {
    ObservedVariable<int,2> var(0,{
        [](const int&){cb1.store(true);},
        [](const int&){cb2.store(true);}
    });

    std::thread wait_thread([&var]() {
        var.wait_until_change();
    });
    std::this_thread::sleep_for(Seconds{2});
    // checks if it is a valid thread
    ASSERT_TRUE(wait_thread.joinable());
    var.set(10);
    wait_thread.join();
    // reaching these next lines proves `wait_until_change` is functional
    ASSERT_TRUE(cb1.load());
    ASSERT_TRUE(cb2.load());
}
