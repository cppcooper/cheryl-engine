#include <gtest/gtest.h>

#include <core/subsystems/event-bus.h>
#include <internals/exceptions.h>

#include <any>
#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

TEST(event_bus, ignoring_an_identifier_keeps_the_registration_alive) {
    CE::SubSystems::EventBus bus;
    int calls = 0;
    bus.register_listener("tick", [&](std::any value) { calls += std::any_cast<int>(value); });
    bus.dispatch("tick", 2);
    bus.dispatch("tick", 3);
    EXPECT_EQ(calls, 5);
}

TEST(event_bus, independent_buses_keep_their_registrations_separate) {
    CE::SubSystems::EventBus first;
    CE::SubSystems::EventBus second;
    int calls = 0;
    first.register_listener("tick", [&](std::any) { ++calls; });
    second.dispatch("tick", 0);
    EXPECT_EQ(calls, 0);
    first.dispatch("tick", 0);
    EXPECT_EQ(calls, 1);
}

TEST(event_bus, immediate_delivery_uses_registration_order_and_the_producer_thread) {
    CE::SubSystems::EventBus bus;
    std::vector<int> order;
    std::thread::id owner;
    bus.register_listener("tick", [&](std::any) { order.push_back(1); owner = std::this_thread::get_id(); });
    bus.register_listener("tick", [&](std::any) { order.push_back(2); });
    std::thread producer([&] { bus.dispatch("tick", 0); });
    const auto producer_id = producer.get_id();
    producer.join();
    EXPECT_EQ(order, (std::vector<int>{1, 2}));
    EXPECT_EQ(owner, producer_id);
}

TEST(event_bus, registration_during_dispatch_joins_the_next_snapshot) {
    CE::SubSystems::EventBus bus;
    std::vector<int> order;
    bool added = false;
    bus.register_listener("tick", [&](std::any) {
        order.push_back(1);
        if (!added) {
            added = true;
            bus.register_listener("tick", [&](std::any) { order.push_back(2); });
        }
    });
    bus.dispatch("tick", 0);
    EXPECT_EQ(order, std::vector<int>{1});
    bus.dispatch("tick", 0);
    EXPECT_EQ(order, (std::vector<int>{1, 1, 2}));
}

TEST(event_bus, unregister_skips_a_listener_already_in_the_dispatch_snapshot) {
    CE::SubSystems::EventBus bus;
    CE::SubSystems::EventBus::Registration second;
    int calls = 0;
    bus.register_listener("tick", [&](std::any) { bus.unregister_listener(second); });
    second = bus.register_listener("tick", [&](std::any) { ++calls; });
    bus.dispatch("tick", 0);
    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(bus.unregister_listener(second));
}

TEST(event_bus, a_registration_cannot_remove_a_listener_on_another_bus) {
    CE::SubSystems::EventBus first;
    CE::SubSystems::EventBus second;
    int calls = 0;
    const auto first_id = first.register_listener("tick", [](std::any) {});
    second.register_listener("tick", [&](std::any) { ++calls; });
    EXPECT_FALSE(second.unregister_listener(first_id));
    second.dispatch("tick", 0);
    EXPECT_EQ(calls, 1);
}

TEST(event_bus, self_unregister_is_safe_but_self_wait_is_rejected) {
    CE::SubSystems::EventBus bus;
    CE::SubSystems::EventBus::Registration id;
    int calls = 0;
    id = bus.register_listener("tick", [&](std::any) {
        ++calls;
        EXPECT_TRUE(bus.unregister_listener(id));
        EXPECT_THROW(bus.wait_for_listener(id), CE::Exceptions::failed_operation);
    });
    bus.dispatch("tick", 0);
    bus.dispatch("tick", 0);
    bus.wait_for_listener(id);
    EXPECT_EQ(calls, 1);
}

TEST(event_bus, unregister_does_not_destroy_a_borrowed_target_until_its_running_callback_finishes) {
    CE::SubSystems::EventBus bus;
    std::promise<void> entered;
    std::promise<void> release;
    auto may_finish = release.get_future().share();
    const auto id = bus.register_listener("tick", [&](std::any) {
        entered.set_value();
        may_finish.wait();
    });
    std::thread producer([&] { bus.dispatch("tick", 0); });
    entered.get_future().wait();
    EXPECT_TRUE(bus.unregister_listener(id));
    // A new dispatch cannot re-enter the blocked callback after unregister.
    bus.dispatch("tick", 0);
    auto barrier = std::async(std::launch::async, [&] { bus.wait_for_listener(id); });
    EXPECT_EQ(barrier.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
    release.set_value();
    producer.join();
    barrier.get();
}

TEST(event_bus, callback_failure_leaves_the_completion_barrier_usable) {
    CE::SubSystems::EventBus bus;
    const auto id = bus.register_listener("tick", [](std::any) { throw std::runtime_error("failed"); });
    EXPECT_THROW(bus.dispatch("tick", 0), std::runtime_error);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(event_bus, close_invalidates_registrations_and_rejects_new_work) {
    CE::SubSystems::EventBus bus;
    const auto id = bus.register_listener("tick", [](std::any) {});
    bus.close();
    bus.wait_for_listener(id);
    EXPECT_THROW(bus.register_listener("tick", [](std::any) {}), CE::Exceptions::failed_operation);
    EXPECT_THROW(bus.dispatch("tick", 0), CE::Exceptions::failed_operation);
}
