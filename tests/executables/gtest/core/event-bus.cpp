#include <gtest/gtest.h>

#include <core/subsystems/event-bus.h>
#include <internals/exceptions.h>

#include <any>
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
