#include <gtest/gtest.h>

#include <core/subsystems/event-bus.h>
#include <core/subsystems/event-system.h>
#include <internals/exceptions.h>

#include <any>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
    template <typename Bus, typename Channel, typename Value>
    concept CanDispatch = requires(Bus& bus, const Channel& channel, Value&& value) {
        bus.dispatch(channel, std::forward<Value>(value));
    };

    template <typename Callback>
    concept CanListen = requires(
        CE::SubSystems::EventBus& bus, const CE::SubSystems::EventChannel<int>& channel, Callback callback
    ) {
        bus.register_listener(channel, callback);
    };

    using IntChannel = CE::SubSystems::EventChannel<int>;
    static_assert(CE::SubSystems::EventPayload<int>);
    static_assert(CE::SubSystems::EventPayload<std::any>);
    static_assert(!CE::SubSystems::EventPayload<const int>);
    static_assert(!CE::SubSystems::EventPayload<int&>);
    static_assert(!CE::SubSystems::EventPayload<std::unique_ptr<int>>);
    static_assert(CanDispatch<CE::SubSystems::EventBus, IntChannel, int>);
    static_assert(CanDispatch<CE::SubSystems::EventBus, IntChannel, const int&>);
    static_assert(!CanDispatch<CE::SubSystems::EventBus, IntChannel, double>);
    static_assert(!CanDispatch<CE::SubSystems::EventBus, IntChannel, std::any>);
    static_assert(!CanDispatch<CE::SubSystems::EventSystem, IntChannel, std::string>);
    static_assert(CanListen<void (*)(const int&)>);
    static_assert(!CanListen<void (*)(const std::string&)>);
}

TEST(typed_events, identity) {
    CE::SubSystems::EventBus bus;
    int integers = 0;
    int strings = 0;
    int named = 0;
    {
        std::string name = "sample";
        const IntChannel channel{name};
        name = "changed";
        // Registration persists after both the channel and ignored ID are released.
        bus.register_listener(channel, [&](const int& value) { integers += value; });
    }
    const IntChannel channel{"sample"};
    const CE::SubSystems::EventChannel<std::string> text{"sample"};
    bus.register_listener(text, [&](const std::string&) { ++strings; });
    bus.register_listener("sample", [&](std::any) { ++named; });
    bus.dispatch(channel, 2);
    bus.dispatch(IntChannel{"sample"}, 3);
    bus.dispatch(IntChannel{"Sample"}, 99);
    EXPECT_EQ(integers, 5);
    EXPECT_EQ(strings, 0);
    EXPECT_EQ(named, 0);
    bus.dispatch(text, std::string{"text"});
    bus.dispatch("sample", 7);
    EXPECT_EQ(integers, 5);
    EXPECT_EQ(strings, 1);
    EXPECT_EQ(named, 1);
}

TEST(typed_events, payload) {
    CE::SubSystems::EventBus bus;
    const CE::SubSystems::EventChannel<std::string> channel{"text"};
    std::string source = "original";
    std::vector<std::string> received;
    bus.register_listener(channel, [&](const std::string& value) {
        source = "changed";
        received.push_back(value);
    });
    bus.register_listener(channel, [&](const std::string& value) { received.push_back(value); });
    bus.dispatch(channel, source);
    EXPECT_EQ(source, "changed");
    EXPECT_EQ(received, (std::vector<std::string>{"original", "original"}));
}

TEST(typed_events, any) {
    CE::SubSystems::EventBus bus;
    const CE::SubSystems::EventChannel<std::any> channel{"erased-value"};
    int received = 0;
    bus.register_listener(channel, [&](const std::any& value) { received = std::any_cast<int>(value); });
    std::any payload = 42;
    bus.dispatch(channel, payload);
    EXPECT_EQ(received, 42);
}

TEST(typed_events, remove) {
    CE::SubSystems::EventBus first;
    CE::SubSystems::EventBus second;
    const IntChannel integer{"sample"};
    const CE::SubSystems::EventChannel<std::string> text{"sample"};
    int calls = 0;
    const auto id = first.register_listener(integer, [&](const int&) { ++calls; });
    first.register_listener(text, [&](const std::string&) { ++calls; });
    EXPECT_FALSE(second.unregister_listener(id));
    EXPECT_TRUE(first.unregister_and_wait(id));
    first.dispatch(integer, 1);
    first.dispatch(text, std::string{"text"});
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(first.unregister_listener(id));
}

TEST(typed_events, nested) {
    CE::SubSystems::EventBus bus;
    const IntChannel channel{"tick"};
    CE::SubSystems::EventBus::Registration first;
    std::vector<int> received;
    first = bus.register_listener(channel, [&](const int& value) {
        received.push_back(value);
        EXPECT_TRUE(bus.unregister_listener(first));
        EXPECT_THROW(bus.wait_for_listener(first), CE::Exceptions::failed_operation);
        bus.dispatch(channel, 2);
    });
    bus.register_listener(channel, [&](const int& value) { received.push_back(value * 10); });
    bus.dispatch(channel, 1);
    bus.wait_for_listener(first);
    EXPECT_EQ(received, (std::vector<int>{1, 20, 10}));
}

TEST(typed_events, errors) {
    CE::SubSystems::EventSystem system;
    const IntChannel channel{"tick"};
    EXPECT_THROW(system.register_listener(channel, IntChannel::Callback{}), CE::Exceptions::invalid_args);
    EXPECT_THROW(
        system.register_listener(channel, [](const int&) {}, [](CE::SubSystems::EventBus::Work) { return false; }),
        CE::Exceptions::invalid_args
    );
    int received = 0;
    const auto id = system.register_listener(channel, [&](const int& value) { received += value; });
    system.dispatch(channel, 4);
    EXPECT_EQ(received, 4);
    EXPECT_TRUE(system.unregister_and_wait(id));
    system.bus().close();
    EXPECT_THROW(system.register_listener(channel, [](const int&) {}), CE::Exceptions::failed_operation);
    EXPECT_THROW(system.dispatch(channel, 5), CE::Exceptions::failed_operation);
}
