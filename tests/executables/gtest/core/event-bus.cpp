#include <gtest/gtest.h>

#include <core/subsystems/event-bus.h>
#include <core/engine/event-delivery.h>
#include <core/engine/event-delivery-internal.h>
#include <core/engine/worker-pool-internal.h>
#include <internals/exceptions.h>

#include <any>
#include <atomic>
#include <chrono>
#include <future>
#include <functional>
#include <latch>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
    // Declare after borrowed recording state so an assertion's early return
    // invalidates tickets before pending pumps destroy their error sinks.
    struct CloseBusOnExit {
        CE::SubSystems::EventBus& bus;
        ~CloseBusOnExit() { bus.close(); }
    };

    class PromiseGate final {
        std::promise<void>& release_;
        bool open_ = false;

    public:
        explicit PromiseGate(
            std::promise<void>& release
        )
        : release_(release) {}
        ~PromiseGate() { open(); }
        void open() {
            if (!open_) {
                release_.set_value();
                open_ = true;
            }
        }
    };

    struct HeldPayloadCopy {
        std::promise<void>& entered;
        std::shared_future<void> may_copy;
        std::function<void()>& release;
        bool copied = false;

        HeldPayloadCopy(
            std::promise<void>& entry,
            std::shared_future<void> gate,
            std::function<void()>& released
        )
        : entered(entry), may_copy(std::move(gate)), release(released) {}
        HeldPayloadCopy(
            const HeldPayloadCopy& other
        )
        : entered(other.entered), may_copy(other.may_copy), release(other.release), copied(true) {
            entered.set_value();
            may_copy.wait();
        }
        ~HeldPayloadCopy() {
            if (copied)
                release();
        }
    };

    struct RedispatchOnCopiedPayloadRelease {
        std::function<void()>& release;
        bool copied = false;

        explicit RedispatchOnCopiedPayloadRelease(
            std::function<void()>& callback
        )
        : release(callback) {}
        RedispatchOnCopiedPayloadRelease(
            const RedispatchOnCopiedPayloadRelease& source
        )
        : release(source.release), copied(true) {}
        ~RedispatchOnCopiedPayloadRelease() {
            if (copied)
                release();
        }
    };
}

TEST(
    event_bus,
    ignored_registration_id
) {
    CE::SubSystems::EventBus bus;
    int calls = 0;
    bus.register_listener("tick", [&](std::any value) { calls += std::any_cast<int>(value); });
    bus.dispatch("tick", 2);
    bus.dispatch("tick", 3);
    EXPECT_EQ(calls, 5);
}

TEST(
    event_bus,
    independent_registrations
) {
    CE::SubSystems::EventBus first;
    CE::SubSystems::EventBus second;
    int calls = 0;
    first.register_listener("tick", [&](std::any) { ++calls; });
    second.dispatch("tick", 0);
    EXPECT_EQ(calls, 0);
    first.dispatch("tick", 0);
    EXPECT_EQ(calls, 1);
}

TEST(
    event_bus,
    immediate_delivery_order
) {
    CE::SubSystems::EventBus bus;
    std::vector<int> order;
    std::thread::id owner;
    bus.register_listener("tick", [&](std::any) {
        order.push_back(1);
        owner = std::this_thread::get_id();
    });
    bus.register_listener("tick", [&](std::any) { order.push_back(2); });
    std::thread producer([&] { bus.dispatch("tick", 0); });
    const auto producer_id = producer.get_id();
    producer.join();
    EXPECT_EQ(order, (std::vector<int>{1, 2}));
    EXPECT_EQ(owner, producer_id);
}

TEST(
    event_bus,
    registration_during_dispatch
) {
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

TEST(
    event_bus,
    unregister_during_dispatch
) {
    CE::SubSystems::EventBus bus;
    CE::SubSystems::EventBus::Registration second;
    int calls = 0;
    bus.register_listener("tick", [&](std::any) { bus.unregister_listener(second); });
    second = bus.register_listener("tick", [&](std::any) { ++calls; });
    bus.dispatch("tick", 0);
    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(bus.unregister_listener(second));
}

TEST(
    event_bus,
    foreign_unregister
) {
    CE::SubSystems::EventBus first;
    CE::SubSystems::EventBus second;
    int calls = 0;
    const auto first_id = first.register_listener("tick", [](std::any) {});
    second.register_listener("tick", [&](std::any) { ++calls; });
    EXPECT_FALSE(second.unregister_listener(first_id));
    second.dispatch("tick", 0);
    EXPECT_EQ(calls, 1);
}

TEST(
    event_bus,
    self_unregister
) {
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

TEST(
    event_bus,
    running_callback_lifetime
) {
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

TEST(
    event_bus,
    callback_failure_barrier
) {
    CE::SubSystems::EventBus bus;
    const auto id = bus.register_listener("tick", [](std::any) { throw std::runtime_error("failed"); });
    EXPECT_THROW(bus.dispatch("tick", 0), std::runtime_error);
    EXPECT_TRUE(bus.unregister_and_wait(id));
}

TEST(
    event_bus,
    closed_bus
) {
    CE::SubSystems::EventBus bus;
    const auto id = bus.register_listener("tick", [](std::any) {});
    bus.close();
    bus.wait_for_listener(id);
    EXPECT_THROW(bus.register_listener("tick", [](std::any) {}), CE::Exceptions::failed_operation);
    EXPECT_THROW(bus.dispatch("tick", 0), CE::Exceptions::failed_operation);
}

namespace {
    struct CopyFailure {
        CopyFailure() = default;
        CopyFailure(
            const CopyFailure&
        ) {
            throw std::runtime_error("payload copy failed");
        }
    };

    struct QueuedDelivery {
        std::vector<CE::SubSystems::EventBus::Work> pending;

        CE::SubSystems::EventBus::Delivery target() {
            return [this](CE::SubSystems::EventBus::Work work) {
                pending.push_back(std::move(work));
                return true;
            };
        }

        void drain() {
            std::vector<CE::SubSystems::EventBus::Work> batch;
            batch.swap(pending);
            for (auto& work : batch)
                work();
        }
    };
}

TEST(
    event_bus,
    queued_payload_order
) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    std::vector<int> received;
    std::vector<std::exception_ptr> errors;
    bus.register_listener(
        "tick", [&](std::any value) { received.push_back(std::any_cast<int>(value)); }, target.target(),
        [&](std::exception_ptr error) { errors.push_back(error); }
    );
    std::any payload = 1;
    bus.dispatch("tick", payload);
    payload = 2;
    bus.dispatch("tick", payload);
    payload = 99;
    EXPECT_TRUE(received.empty());
    target.drain();
    EXPECT_EQ(received, (std::vector<int>{1, 2}));
    EXPECT_TRUE(errors.empty());
}

TEST(
    event_bus,
    queued_unregister
) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    int calls = 0;
    int errors = 0;
    const auto id = bus.register_listener("tick", [&](std::any) { ++calls; }, target.target(), [&](std::exception_ptr) { ++errors; });
    bus.dispatch("tick", 0);
    EXPECT_TRUE(bus.unregister_and_wait(id));
    target.drain();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(errors, 0);
}

TEST(
    event_bus,
    close_during_dispatch
) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    std::promise<void> entered;
    std::promise<void> release;
    auto may_finish = release.get_future().share();
    int queued_calls = 0;
    int errors = 0;
    const auto running = bus.register_listener("running", [&](std::any) {
        entered.set_value();
        may_finish.wait();
    });
    bus.register_listener("queued", [&](std::any) { ++queued_calls; }, target.target(), [&](std::exception_ptr) { ++errors; });
    bus.dispatch("queued", 0);
    std::thread producer([&] { bus.dispatch("running", 0); });
    entered.get_future().wait();
    std::promise<void> close_start;
    auto may_close = close_start.get_future().share();
    auto first = std::async(std::launch::async, [&] {
        may_close.wait();
        bus.close();
    });
    auto second = std::async(std::launch::async, [&] {
        may_close.wait();
        bus.close();
    });
    close_start.set_value();
    first.get();
    second.get();
    target.drain();
    release.set_value();
    producer.join();
    bus.wait_for_listener(running);
    EXPECT_EQ(queued_calls, 0);
    EXPECT_EQ(errors, 0);
}

TEST(
    event_bus,
    capture_release_reentry
) {
    for (const bool close_bus : {false, true}) {
        CE::SubSystems::EventBus bus;
        bool released = false;
        auto owned = std::shared_ptr<int>(new int{0}, [&](int* value) {
            delete value;
            released = true;
            // Reentry would deadlock if removal destroyed captures under a lock.
            bus.close();
        });
        const auto id = bus.register_listener("tick", [owned](std::any) {});
        owned.reset();
        if (close_bus)
            bus.close();
        else
            EXPECT_TRUE(bus.unregister_listener(id));
        EXPECT_TRUE(released);
    }
}

TEST(
    event_bus,
    queued_error_reporting
) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    std::vector<std::exception_ptr> errors;
    bus.register_listener(
        "tick", [](std::any) { throw std::runtime_error("callback failed"); }, target.target(),
        [&](std::exception_ptr error) { errors.push_back(error); }
    );
    bus.dispatch("tick", 0);
    target.pending.clear();
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_THROW(std::rethrow_exception(errors[0]), std::future_error);
    bus.dispatch("tick", 0);
    target.drain();
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_THROW(std::rethrow_exception(errors[1]), std::runtime_error);
}

TEST(
    event_bus,
    delivery_rejection_reentry
) {
    CE::SubSystems::EventBus bus;
    CE::SubSystems::EventBus::Registration id;
    int failures = 0;
    id = bus.register_listener(
        "tick", [](std::any) {}, [](CE::SubSystems::EventBus::Work) { return false; },
        [&](std::exception_ptr error) {
            ++failures;
            EXPECT_THROW(std::rethrow_exception(error), std::future_error);
            // Reporting may safely remove the listener which failed delivery.
            EXPECT_TRUE(bus.unregister_listener(id));
        }
    );
    bus.dispatch("tick", 0);
    EXPECT_EQ(failures, 1);
}

TEST(
    event_bus,
    missing_error_sink
) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    EXPECT_THROW(bus.register_listener("tick", [](std::any) {}, target.target()), CE::Exceptions::invalid_args);
}

TEST(
    event_bus,
    payload_copy_failure_reentry
) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    CE::SubSystems::EventBus::Registration id;
    int failures = 0;
    int recovered = 0;
    bus.register_listener("recovery", [&](std::any) { ++recovered; });
    id = bus.register_listener(
        "tick", [](std::any) { ADD_FAILURE() << "Uncopyable payload entered callback"; }, target.target(),
        [&](std::exception_ptr error) {
            ++failures;
            try {
                std::rethrow_exception(error);
            } catch (const std::runtime_error& failure) {
                EXPECT_EQ(std::string(failure.what()), "payload copy failed");
            } catch (...) {
                ADD_FAILURE() << "Preparation reported a different failure";
            }
            EXPECT_TRUE(bus.unregister_listener(id));
            bus.dispatch("recovery", 1);
        }
    );
    const std::any payload(std::in_place_type<CopyFailure>);
    EXPECT_NO_THROW(bus.dispatch("tick", payload));
    EXPECT_EQ(failures, 1);
    EXPECT_EQ(recovered, 1);
    EXPECT_TRUE(target.pending.empty());
}

TEST(
    event_bus,
    delivery_target_failure
) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    bool reject = true;
    int failures = 0;
    int calls = 0;
    bus.register_listener(
        "tick", [&](std::any) { ++calls; },
        [&](CE::SubSystems::EventBus::Work work) {
            if (reject)
                throw std::runtime_error("target unavailable");
            return target.target()(std::move(work));
        },
        [&](std::exception_ptr error) {
            ++failures;
            EXPECT_THROW(std::rethrow_exception(error), std::runtime_error);
        }
    );
    bus.dispatch("tick", 0);
    EXPECT_EQ(failures, 1);
    reject = false;
    bus.dispatch("tick", 0);
    target.drain();
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(failures, 1);
}

TEST(
    event_bus,
    rejected_payload_reentry
) {
    for (const bool throwing : {false, true}) {
        CE::SubSystems::EventBus bus;
        QueuedDelivery target;
        int offers = 0;
        int failures = 0;
        int releases = 0;
        std::vector<int> received;
        std::function<void()> release = [&] {
            ++releases;
            bus.dispatch("tick", 2);
        };
        CloseBusOnExit cleanup{bus};
        bus.register_listener(
            "tick", [&](std::any value) { received.push_back(std::any_cast<int>(value)); },
            [&](CE::SubSystems::EventBus::Work work) {
                if (++offers == 1) {
                    if (throwing)
                        throw CE::Exceptions::failed_operation(CE_HERE, "The target rejected its first payload");
                    return false; // Destroys this target's work before returning to the bus.
                }
                return target.target()(std::move(work));
            },
            [&](std::exception_ptr error) {
                ++failures;
                if (throwing)
                    EXPECT_THROW(std::rethrow_exception(error), CE::Exceptions::failed_operation);
                else
                    EXPECT_THROW(std::rethrow_exception(error), std::future_error);
            }
        );

        // Only the bus's copy redispatches on release; the producer's original
        // stays unarmed. This exercises payload destruction, not sink reentry.
        const std::any payload(std::in_place_type<RedispatchOnCopiedPayloadRelease>, release);
        bus.dispatch("tick", payload);
        EXPECT_EQ(releases, 1);
        EXPECT_EQ(failures, 1);
        EXPECT_EQ(offers, 2);
        EXPECT_TRUE(received.empty());
        target.drain();
        EXPECT_EQ(received, (std::vector<int>{2}));
        EXPECT_EQ(failures, 1);
    }
}

TEST(
    event_bus,
    worker_delivery_order
) {
    CE::Engine::WorkerPool pool(3);
    auto group = pool.make_group();
    auto delivery = CE::Engine::worker_event_delivery(group);
    CE::SubSystems::EventBus bus;
    std::vector<int> received;
    std::atomic<int> errors{0};
    bus.register_listener(
        "tick", [&](std::any value) { received.push_back(std::any_cast<int>(value)); }, delivery, [&](std::exception_ptr) { ++errors; }
    );
    for (int value = 0; value < 16; ++value)
        bus.dispatch("tick", value);
    group.close();
    group.drain();
    ASSERT_EQ(received.size(), 16u);
    for (int value = 0; value < 16; ++value)
        EXPECT_EQ(received[static_cast<std::size_t>(value)], value);
    EXPECT_EQ(errors.load(), 0);
    bus.close();
}

TEST(
    event_bus,
    shared_worker_stream
) {
    CE::Engine::WorkerPool pool(3);
    auto group = pool.make_group();
    auto delivery = CE::Engine::worker_event_delivery(group);
    CE::SubSystems::EventBus bus;
    std::vector<int> order;
    const auto report = [](std::exception_ptr) { ADD_FAILURE() << "Unexpected worker delivery error"; };
    bus.register_listener("tick", [&](std::any) { order.push_back(1); }, delivery, report);
    bus.register_listener("tick", [&](std::any) { order.push_back(2); }, delivery, report);
    bus.dispatch("tick", 0);
    bus.dispatch("tick", 0);
    group.close();
    group.drain();
    EXPECT_EQ(order, (std::vector<int>{1, 2, 1, 2}));
    bus.close();
}

TEST(
    event_bus,
    expired_worker_target
) {
    auto pool = std::make_unique<CE::Engine::WorkerPool>();
    auto delivery = CE::Engine::worker_event_delivery(pool->make_group());
    CE::SubSystems::EventBus bus;
    int errors = 0;
    bus.register_listener("tick", [](std::any) {}, delivery, [&](std::exception_ptr) { ++errors; });
    pool.reset();
    bus.dispatch("tick", 0);
    EXPECT_EQ(errors, 1);
    bus.close();
}

TEST(
    event_bus,
    closed_worker_group
) {
    CE::Engine::WorkerPool pool(2);
    auto group = pool.make_group();
    const auto delivery = CE::Engine::worker_event_delivery(group);
    CE::SubSystems::EventBus bus;
    std::promise<void> entered;
    auto started = entered.get_future();
    std::promise<void> release;
    auto may_finish = release.get_future().share();
    std::vector<int> received;
    const auto unexpected = [](std::exception_ptr) { ADD_FAILURE() << "Accepted delivery was lost"; };
    bus.register_listener(
        "accepted",
        [&](std::any value) {
            if (std::any_cast<int>(value) == 0) {
                entered.set_value();
                may_finish.wait();
            }
            received.push_back(std::any_cast<int>(value));
        },
        delivery, unexpected
    );
    bus.dispatch("accepted", 0);
    EXPECT_EQ(started.wait_for(std::chrono::seconds{1}), std::future_status::ready);
    bus.dispatch("accepted", 1); // The same pump owns this queued callback.
    group.close();

    int errors = 0;
    int immediate = 0;
    CE::SubSystems::EventBus::Registration rejected;
    bus.register_listener("report", [&](std::any) { ++immediate; });
    rejected = bus.register_listener(
        "rejected", [](std::any) { ADD_FAILURE() << "Closed group invoked a new callback"; }, delivery,
        [&](std::exception_ptr error) {
            EXPECT_THROW(std::rethrow_exception(error), std::future_error);
            ++errors;
            bus.dispatch("report", 0);
            EXPECT_TRUE(bus.unregister_listener(rejected));
        }
    );
    bus.dispatch("rejected", 2);
    EXPECT_EQ(errors, 1);
    EXPECT_EQ(immediate, 1);
    release.set_value();
    group.drain();
    EXPECT_EQ(received, (std::vector<int>{0, 1}));
    bus.close();
}

TEST(
    event_bus,
    independent_worker_streams
) {
    CE::Engine::WorkerPool pool(2);
    auto group = pool.make_group();
    CE::SubSystems::EventBus bus;
    std::promise<void> entered;
    auto started = entered.get_future();
    std::promise<void> release;
    auto may_finish = release.get_future().share();
    std::promise<void> independent;
    auto progressed = independent.get_future();
    const auto unexpected = [](std::exception_ptr) { ADD_FAILURE() << "Unexpected worker delivery error"; };
    bus.register_listener(
        "held",
        [&](std::any) {
            entered.set_value();
            may_finish.wait();
        },
        CE::Engine::worker_event_delivery(group), unexpected
    );
    bus.register_listener("independent", [&](std::any) { independent.set_value(); }, CE::Engine::worker_event_delivery(group), unexpected);
    bus.dispatch("held", 0);
    EXPECT_EQ(started.wait_for(std::chrono::seconds{1}), std::future_status::ready);
    bus.dispatch("independent", 0);
    // A bounded wait checks progress; the promise establishes the held state.
    EXPECT_EQ(progressed.wait_for(std::chrono::seconds{1}), std::future_status::ready);
    release.set_value();
    group.close();
    group.drain();
    bus.close();
}

TEST(
    event_bus,
    unpublished_pump_recovery
) {
    CE::Engine::WorkerPool pool;
    auto group = pool.make_group();
    CE::SubSystems::EventBus bus;
    std::vector<CE::SubSystems::EventBus::Work> pumps;
    int offers = 0;
    auto delivery = CE::Engine::DeliveryDetail::worker_stream_delivery(group, [&](CE::SubSystems::EventBus::Work pump) {
        if (++offers > 1)
            pumps.push_back(std::move(pump));
        // The first accepted callable is discarded before publication,
        // modeling native rejection before entry without invoking callbacks.
    });
    int errors = 0;
    std::vector<int> received;
    CloseBusOnExit cleanup{bus};
    bus.register_listener(
        "tick", [&](std::any value) { received.push_back(std::any_cast<int>(value)); }, delivery,
        [&](std::exception_ptr error) {
            EXPECT_THROW(std::rethrow_exception(error), std::future_error);
            ++errors;
            if (errors == 1)
                bus.dispatch("tick", 2); // Must run outside this listener's posting lock.
        }
    );
    bus.dispatch("tick", 1);
    EXPECT_EQ(errors, 1);
    EXPECT_EQ(offers, 2);
    EXPECT_TRUE(received.empty());
    ASSERT_EQ(pumps.size(), 1u);
    auto pump = std::move(pumps.front());
    pumps.clear();
    pump(); // Explicit execution happens only after submission has returned.
    EXPECT_EQ(received, (std::vector<int>{2}));
    bus.dispatch("tick", 3);
    EXPECT_EQ(offers, 3);
    ASSERT_EQ(pumps.size(), 1u);
    pumps.front()();
    pumps.clear();
    EXPECT_EQ(received, (std::vector<int>{2, 3}));
    EXPECT_EQ(errors, 1);
}

TEST(
    event_bus,
    published_pump_recovery
) {
    CE::Engine::WorkerPool pool;
    auto group = pool.make_group();
    CE::SubSystems::EventBus bus;
    std::vector<CE::SubSystems::EventBus::Work> pumps;
    auto delivery = CE::Engine::DeliveryDetail::worker_stream_delivery(group,
        [&](CE::SubSystems::EventBus::Work pump) { pumps.push_back(std::move(pump)); });
    int errors = 0;
    int reports = 0;
    std::vector<int> received;
    std::thread::id cancellation_thread;
    CloseBusOnExit cleanup{bus};
    bus.register_listener("report", [&](std::any) { ++reports; });
    const auto on_error = [&](std::exception_ptr error) {
        EXPECT_THROW(std::rethrow_exception(error), std::future_error);
        EXPECT_EQ(std::this_thread::get_id(), cancellation_thread);
        ++errors;
        bus.dispatch("report", 0); // Reporting reenters after the stream lock is released.
    };
    bus.register_listener("first", [&](std::any value) { received.push_back(std::any_cast<int>(value)); }, delivery, on_error);
    bus.register_listener("second", [&](std::any value) { received.push_back(std::any_cast<int>(value)); }, delivery, on_error);
    bus.dispatch("first", 1);
    bus.dispatch("second", 2);
    ASSERT_EQ(pumps.size(), 1u); // Both listeners have joined the same accepted pump.
    auto cancelled = std::move(pumps.front());
    pumps.clear();
    auto worker = std::async(std::launch::async, [&, cancelled = std::move(cancelled)]() mutable {
        cancellation_thread = std::this_thread::get_id();
        cancelled = nullptr;
    });
    worker.get();
    EXPECT_EQ(errors, 2);
    EXPECT_EQ(reports, 2);
    EXPECT_TRUE(received.empty());
    bus.dispatch("first", 3);
    ASSERT_EQ(pumps.size(), 1u);
    pumps.front()();
    pumps.clear();
    EXPECT_EQ(received, (std::vector<int>{3}));
    EXPECT_EQ(errors, 2);
}

TEST(
    event_bus,
    pump_submission_failure
) {
    CE::Engine::WorkerPool pool;
    auto group = pool.make_group();
    CE::SubSystems::EventBus bus;
    std::vector<CE::SubSystems::EventBus::Work> pumps;
    bool reject = true;
    auto delivery = CE::Engine::DeliveryDetail::worker_stream_delivery(group, [&](CE::SubSystems::EventBus::Work pump) {
        if (reject)
            throw CE::Exceptions::failed_operation(CE_HERE, "Controlled pump submission rejection");
        pumps.push_back(std::move(pump));
    });
    int errors = 0;
    std::vector<int> received;
    CloseBusOnExit cleanup{bus};
    bus.register_listener(
        "tick", [&](std::any value) { received.push_back(std::any_cast<int>(value)); }, delivery,
        [&](std::exception_ptr error) {
            EXPECT_THROW(std::rethrow_exception(error), CE::Exceptions::failed_operation);
            ++errors;
            reject = false;
            if (errors == 1)
                bus.dispatch("tick", 2);
        }
    );
    bus.dispatch("tick", 1);
    EXPECT_EQ(errors, 1);
    EXPECT_TRUE(received.empty());
    ASSERT_EQ(pumps.size(), 1u);
    pumps.front()();
    pumps.clear();
    EXPECT_EQ(received, (std::vector<int>{2}));
    EXPECT_EQ(errors, 1);
}

TEST(
    event_bus,
    concurrent_producer_order
) {
    CE::SubSystems::EventBus bus;
    std::vector<std::pair<int, int>> received;
    std::atomic<int> errors{0};
    std::latch ready{2};
    std::promise<void> release;
    auto may_dispatch = release.get_future().share();
    CE::Engine::WorkerPool pool(3);
    auto group = pool.make_group();
    bus.register_listener(
        "tick", [&](std::any value) { received.push_back(std::any_cast<std::pair<int, int>>(value)); },
        CE::Engine::worker_event_delivery(group), [&](std::exception_ptr) { ++errors; }
    );
    CloseBusOnExit cleanup{bus};
    auto produce = [&](int producer) {
        ready.count_down();
        may_dispatch.wait();
        for (int sequence = 0; sequence < 32; ++sequence)
            bus.dispatch("tick", std::pair{producer, sequence});
    };
    std::jthread first(produce, 0);
    PromiseGate start_gate{release};
    std::jthread second(produce, 1);
    ready.wait();
    start_gate.open();
    first.join();
    second.join();
    group.close();
    group.drain();
    ASSERT_EQ(received.size(), 64u);
    int next[2]{};
    for (const auto& [producer, sequence] : received) {
        ASSERT_GE(producer, 0);
        ASSERT_LT(producer, 2);
        EXPECT_EQ(sequence, next[producer]++);
    }
    EXPECT_EQ(next[0], 32);
    EXPECT_EQ(next[1], 32);
    EXPECT_EQ(errors.load(), 0);
    // The interleaving between producers is intentionally unspecified.
}

TEST(
    event_bus,
    invalidation_during_copy
) {
    CE::SubSystems::EventBus bus;
    CE::SubSystems::EventBus::Registration id;
    int calls = 0;
    int errors = 0;
    int releases = 0;
    std::promise<void> entered;
    std::promise<void> release;
    auto may_copy = release.get_future().share();
    std::function<void()> copied_release = [&] {
        ++releases;
        // The ticket's payload destructor must not hold listener/registry locks.
        EXPECT_FALSE(bus.unregister_listener(id));
    };
    QueuedDelivery target;
    id = bus.register_listener("tick", [&](std::any) { ++calls; }, target.target(), [&](std::exception_ptr) { ++errors; });
    const std::any payload(std::in_place_type<HeldPayloadCopy>, entered, may_copy, copied_release);
    auto producer = std::async(std::launch::async, [&] { bus.dispatch("tick", payload); });
    PromiseGate gate{release};
    CloseBusOnExit cleanup{bus};
    entered.get_future().wait();
    EXPECT_TRUE(bus.unregister_and_wait(id));
    EXPECT_EQ(calls, 0); // Removal does not wait for preparation or queued entry.
    gate.open();
    producer.get();
    target.drain();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(errors, 0);
    EXPECT_EQ(releases, 1);
}

TEST(
    event_bus,
    pump_policy_recovery
) {
    CE::SubSystems::EventBus bus;
    std::promise<void> policy_entered;
    std::promise<void> release;
    auto may_fail = release.get_future().share();
    std::promise<void> cancelled;
    int errors = 0;
    int first_errors = 0;
    int second_calls = 0;
    int attempts = 0;
    std::vector<unsigned int> mask{2, 7};
    std::vector<int> received;
    std::thread::id reporting_thread;
    const auto producer_thread = std::this_thread::get_id();
    CE::Engine::WorkerDetail::WorkerNativeAdapter adapter;
    adapter.cpu_affinity = true;
    adapter.query_affinity = [&] { return mask; };
    adapter.set_affinity = [&](const std::vector<unsigned int>& requested) {
        if (++attempts == 1) {
            policy_entered.set_value();
            may_fail.wait();
            throw CE::Exceptions::failed_operation(CE_HERE, "Controlled pump policy rejection");
        }
        mask = requested;
    };
    adapter.start_thread = [](std::function<void()> work) { return std::thread(std::move(work)); };
    auto pool = CE::Engine::WorkerDetail::WorkerPoolAccess::create(1, std::move(adapter));
    CE::Engine::WorkerGroupOptions options;
    options.cpu.cpus = {2};
    options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
    auto group = pool->make_group(options);
    auto delivery = CE::Engine::worker_event_delivery(group);
    PromiseGate gate{release};
    CloseBusOnExit cleanup{bus};
    auto report = [&](std::exception_ptr error) {
        EXPECT_THROW(std::rethrow_exception(error), std::future_error);
        reporting_thread = std::this_thread::get_id();
        if (++errors == 2)
            cancelled.set_value();
    };
    bus.register_listener(
        "first", [&](std::any value) { received.push_back(std::any_cast<int>(value)); }, delivery,
        [&](std::exception_ptr error) {
            if (++first_errors == 1)
                bus.dispatch("first", 3); // Reentry starts a fresh accepted pump.
            report(error);
        }
    );
    bus.register_listener("second", [&](std::any) { ++second_calls; }, delivery, report);
    struct JoinPolicyOnExit {
        CE::Engine::WorkerPool& pool;
        PromiseGate& gate;
        CE::SubSystems::EventBus& bus;

        ~JoinPolicyOnExit() {
            // Keep report's borrowed state alive while rejection cleanup can
            // reenter an open bus; group closure makes any new offer reject.
            gate.open();
            pool.shutdown();
            bus.close();
        }
    } finish{*pool, gate, bus};
    bus.dispatch("first", 1);
    policy_entered.get_future().wait();
    // The first offer returned, so its pump is published before another listener joins.
    bus.dispatch("second", 2);
    gate.open();
    cancelled.get_future().wait();
    group.close();
    group.drain();
    EXPECT_EQ(errors, 2);
    EXPECT_EQ(first_errors, 1);
    EXPECT_EQ(second_calls, 0);
    EXPECT_EQ(received, (std::vector<int>{3}));
    EXPECT_NE(reporting_thread, producer_thread);
    EXPECT_EQ(attempts, 2);
    EXPECT_EQ(group.status().policy_failures, 1u);
    EXPECT_EQ(group.status().accepted, 2u);
    EXPECT_EQ(group.status().completed, 2u);
}
