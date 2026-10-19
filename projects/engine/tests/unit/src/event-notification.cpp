#include <core/subsystems/event-bus.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    template <typename Value>
    concept CanNotify = requires(CE::Events::EventBus& bus, const CE::Events::EventChannel<int>& event, Value&& value) {
        bus.notify(event, std::forward<Value>(value));
    };
    static_assert(CanNotify<int>);
    static_assert(CanNotify<const int&>);
    static_assert(!CanNotify<double>);

    struct CopyFault {
        std::shared_ptr<int> copies;
        int fail_at;

        CopyFault(std::shared_ptr<int> copies, int fail_at)
        : copies(std::move(copies)), fail_at(fail_at) {}
        CopyFault(const CopyFault& other)
        : copies(other.copies), fail_at(other.fail_at) {
            if (++*copies == fail_at)
                throw std::runtime_error("notification payload copy");
        }
    };
}

TEST(event_notification, isolated) {
    CE::Events::EventBus bus;
    const CE::Events::EventChannel<int> channel{"sample"};
    int later = 0;
    std::vector<std::exception_ptr> errors;
    bus.register_listener(channel, [](const int&) { throw std::runtime_error("observer"); });
    bus.register_listener(channel, [&](const int& value) { later += value; });
    EXPECT_THROW(bus.dispatch(channel, 1), std::runtime_error);
    EXPECT_EQ(later, 0);
    bus.notify(channel, 2, [&](std::exception_ptr failure) { errors.push_back(failure); });
    EXPECT_EQ(later, 2);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_THROW(std::rethrow_exception(errors.front()), std::runtime_error);
}

TEST(event_notification, preparation) {
    CE::Events::EventBus bus;
    const CE::Events::EventChannel<CopyFault> channel{"copy"};
    int calls = 0;
    int errors = 0;
    bus.register_listener(channel, [&](const CopyFault&) { ++calls; });
    const CopyFault payload{std::make_shared<int>(0), 1};
    bus.notify(channel, payload, [&](std::exception_ptr) { ++errors; });
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(errors, 1);
}

TEST(event_notification, listener_copy) {
    CE::Events::EventBus bus;
    const CE::Events::EventChannel<CopyFault> channel{"copy"};
    int calls = 0;
    int errors = 0;
    bus.register_listener(channel, [&](const CopyFault&) { ++calls; });
    bus.register_listener(channel, [&](const CopyFault&) { ++calls; });
    const CopyFault payload{std::make_shared<int>(0), 2};
    bus.notify(channel, payload, [&](std::exception_ptr) { ++errors; });
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(errors, 1);
}

TEST(event_notification, invalidation) {
    CE::Events::EventBus bus;
    const CE::Events::EventChannel<int> channel{"sample"};
    CE::Events::EventBus::Registration removed;
    int calls = 0;
    bus.register_listener(channel, [&](const int&) { EXPECT_TRUE(bus.unregister_listener(removed)); });
    removed = bus.register_listener(channel, [&](const int&) { ++calls; });
    bus.register_listener(channel, [&](const int&) { ++calls; });
    bus.notify(channel, 1);
    EXPECT_EQ(calls, 1);
}

TEST(event_notification, error_sink) {
    CE::Events::EventBus bus;
    const CE::Events::EventChannel<int> channel{"sample"};
    int calls = 0;
    bus.register_listener(channel, [](const int&) { throw std::runtime_error("observer"); });
    bus.register_listener(channel, [&](const int&) { ++calls; });
    EXPECT_NO_THROW(bus.notify(channel, 1, [](std::exception_ptr) { throw std::runtime_error("sink"); }));
    EXPECT_EQ(calls, 1);
    bus.close();
    std::exception_ptr failure;
    bus.notify(channel, 2, [&](std::exception_ptr next) { failure = next; });
    ASSERT_NE(failure, nullptr);
    EXPECT_THROW(std::rethrow_exception(failure), CE::Exceptions::failed_operation);
}

TEST(event_notification, deferred) {
    CE::Events::EventBus bus;
    const CE::Events::EventChannel<int> channel{"sample"};
    std::vector<CE::Events::EventBus::Work> pending;
    int queued_errors = 0;
    int immediate_errors = 0;
    bus.register_listener(
        channel, [](const int&) { throw std::runtime_error("deferred observer"); },
        [&](CE::Events::EventBus::Work work) {
            pending.push_back(std::move(work));
            return true;
        },
        [&](std::exception_ptr) { ++queued_errors; }
    );
    bus.notify(channel, 1, [&](std::exception_ptr) { ++immediate_errors; });
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(queued_errors, 0);
    pending.front()();
    EXPECT_EQ(queued_errors, 1);
    EXPECT_EQ(immediate_errors, 0);
    pending.clear();
    bus.notify(channel, 2, [&](std::exception_ptr) { ++immediate_errors; });
    pending.clear();
    EXPECT_EQ(queued_errors, 2); // Accepted work discarded by its target reports cancellation.
    EXPECT_EQ(immediate_errors, 0);
}
