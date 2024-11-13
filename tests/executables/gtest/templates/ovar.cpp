#include <gtest/gtest.h>
#include <templates/observed-variables.h>
#include <thread>
#include <math/time.h>

std::atomic_bool cb1{false};
std::atomic_bool cb2{false};

TEST(templates_ovar, simple) {
    // todo: test callbacks. how to perform assertions? future promise?
    // todo: test wait_for_change. make change on a new thread, and wait inside the TEST
    //       (though.. maybe vice versa.. and thread.is_joinable() as the assert condition)
    // todo: how to incorporate threading? callbacks could be thrown on threads..
    ObservedVariable<int,2> var(0,{
        [](const int&){cb1.store(true);},
        [](const int&){cb2.store(true);}
    });

    std::thread wait_thread([&var]() {
        var.wait_until_change();
    });
    std::this_thread::sleep_for(Seconds{2});
    ASSERT_TRUE(wait_thread.joinable());
    var.set(10);
    wait_thread.join();
    // reaching these next lines asserts `wait_until_change` as functional
    ASSERT_TRUE(cb1.load());
    ASSERT_TRUE(cb2.load());
}
