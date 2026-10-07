#include <gtest/gtest.h>

#include <core/engine/event-delivery.h>
#include <core/subsystems/event-bus.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    struct QueuedTarget {
        std::vector<CE::SubSystems::EventBus::Work> pending;

        CE::SubSystems::EventBus::Delivery delivery() {
            return [this](CE::SubSystems::EventBus::Work work) {
                pending.push_back(std::move(work));
                return true;
            };
        }
        void drain() {
            auto batch = std::exchange(pending, {});
            for (auto& work : batch)
                work();
        }
    };

    struct CopyFault {
        std::shared_ptr<bool> fail;

        explicit CopyFault(std::shared_ptr<bool> fail)
        : fail(std::move(fail)) {}
        CopyFault(CopyFault&&) noexcept = default;
        CopyFault(const CopyFault& other)
        : fail(other.fail) {
            if (*fail)
                throw std::runtime_error("payload copy failed");
        }
    };

    class PromiseGate final {
        std::promise<void>& release_;
        bool open_ = false;

    public:
        explicit PromiseGate(std::promise<void>& release)
        : release_(release) {}
        ~PromiseGate() { open(); }
        void open() {
            if (!open_) {
                release_.set_value();
                open_ = true;
            }
        }
    };
}

TEST(typed_events, queued) {
    CE::SubSystems::EventBus bus;
    QueuedTarget target;
    const CE::SubSystems::EventChannel<std::string> channel{"text"};
    auto received = std::make_shared<std::vector<std::string>>();
    auto errors = std::make_shared<std::vector<std::exception_ptr>>();
    const auto id = bus.register_listener(
        channel, [received](const std::string& value) { received->push_back(value); }, target.delivery(),
        [errors](std::exception_ptr error) { errors->push_back(error); }
    );
    std::string source = "first";
    bus.dispatch(channel, source);
    source = "second";
    bus.dispatch(channel, source);
    source = "changed";
    EXPECT_TRUE(received->empty());
    target.drain();
    EXPECT_EQ(*received, (std::vector<std::string>{"first", "second"}));
    bus.dispatch(channel, source);
    EXPECT_TRUE(bus.unregister_and_wait(id));
    target.drain();
    EXPECT_EQ(received->size(), 2u);
    EXPECT_TRUE(errors->empty());
}

TEST(typed_events, copy_failure) {
    CE::SubSystems::EventBus bus;
    QueuedTarget target;
    const CE::SubSystems::EventChannel<CopyFault> channel{"copy"};
    auto fail = std::make_shared<bool>(true);
    auto errors = std::make_shared<std::vector<std::exception_ptr>>();
    bus.register_listener(
        channel, [](const CopyFault&) { ADD_FAILURE() << "Failed payload must not reach the callback"; }, target.delivery(),
        [errors](std::exception_ptr error) { errors->push_back(error); }
    );
    CopyFault source{fail};
    EXPECT_THROW(bus.dispatch(channel, source), std::runtime_error); // Initial producer copy.
    EXPECT_TRUE(errors->empty());
    EXPECT_NO_THROW(bus.dispatch(channel, CopyFault{fail})); // Move into envelope; queued copy fails.
    ASSERT_EQ(errors->size(), 1u);
    EXPECT_THROW(std::rethrow_exception(errors->front()), std::runtime_error);
    EXPECT_TRUE(target.pending.empty());
}

TEST(typed_events, cancellation) {
    CE::SubSystems::EventBus bus;
    QueuedTarget target;
    const CE::SubSystems::EventChannel<int> channel{"tick"};
    auto errors = std::make_shared<std::vector<std::exception_ptr>>();
    bus.register_listener(
        channel, [](const int&) { throw std::runtime_error("callback failed"); }, target.delivery(),
        [errors](std::exception_ptr error) { errors->push_back(error); }
    );
    bus.dispatch(channel, 1);
    target.drain();
    ASSERT_EQ(errors->size(), 1u);
    EXPECT_THROW(std::rethrow_exception(errors->front()), std::runtime_error);
    bus.dispatch(channel, 2);
    target.pending.clear();
    ASSERT_EQ(errors->size(), 2u);
    try {
        std::rethrow_exception(errors->back());
        FAIL() << "Cancelled task must report a broken promise";
    } catch (const std::future_error& error) {
        EXPECT_EQ(error.code(), std::make_error_code(std::future_errc::broken_promise));
    }
    bus.dispatch(channel, 3);
    bus.close();
    target.pending.clear();
    EXPECT_EQ(errors->size(), 2u); // Explicit invalidation discards without reporting loss.
}

TEST(typed_events, error_owner) {
    CE::SubSystems::EventBus bus;
    QueuedTarget target;
    const CE::SubSystems::EventChannel<int> channel{"tick"};
    auto errors = std::make_shared<std::vector<std::exception_ptr>>();
    const std::weak_ptr retained = errors;
    const auto id = bus.register_listener(
        channel, [](const int&) {}, target.delivery(), [errors](std::exception_ptr error) { errors->push_back(error); }
    );
    bus.dispatch(channel, 1);
    errors.reset();
    EXPECT_TRUE(bus.unregister_and_wait(id));
    EXPECT_FALSE(retained.expired()); // Invalidated pending work still owns its error handler.
    target.pending.clear();
    EXPECT_TRUE(retained.expired());
}

TEST(typed_events, wait) {
    CE::SubSystems::EventBus bus;
    const CE::SubSystems::EventChannel<int> channel{"held"};
    std::promise<void> entered;
    std::promise<void> release;
    const auto may_finish = release.get_future().share();
    const auto id = bus.register_listener(channel, [&](const int&) {
        entered.set_value();
        may_finish.wait();
    });
    auto producer = std::async(std::launch::async, [&] { bus.dispatch(channel, 1); });
    PromiseGate gate{release}; // Release before producer destruction on an early return.
    ASSERT_EQ(entered.get_future().wait_for(std::chrono::seconds{2}), std::future_status::ready);
    EXPECT_TRUE(bus.unregister_listener(id));
    bus.dispatch(channel, 2);
    auto barrier = std::async(std::launch::async, [&] { bus.wait_for_listener(id); });
    EXPECT_EQ(barrier.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
    gate.open();
    producer.get();
    barrier.get();
}

TEST(typed_events, fifo) {
    CE::Engine::WorkerPool pool{2};
    auto group = pool.make_group();
    CE::SubSystems::EventBus bus;
    const CE::SubSystems::EventChannel<int> channel{"tick"};
    auto received = std::make_shared<std::vector<int>>();
    auto errors = std::make_shared<std::atomic<unsigned int>>(0);
    bus.register_listener(
        channel, [received](const int& value) { received->push_back(value); }, CE::Engine::worker_event_delivery(group),
        [errors](std::exception_ptr) { ++*errors; }
    );
    for (int value = 0; value < 16; ++value)
        bus.dispatch(channel, value);
    group.close();
    group.drain();
    std::vector<int> expected;
    for (int value = 0; value < 16; ++value)
        expected.push_back(value);
    EXPECT_EQ(*received, expected);
    EXPECT_EQ(errors->load(), 0u);
    EXPECT_NO_THROW(bus.dispatch(channel, 16));
    EXPECT_EQ(*received, expected);
    EXPECT_EQ(errors->load(), 1u); // Closed worker target rejects through the owned error sink.
}
