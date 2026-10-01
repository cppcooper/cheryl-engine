#include <gtest/gtest.h>

#include <core/subsystems/event-bus.h>
#include <core/engine/event-delivery.h>
#include <internals/exceptions.h>

#include <any>
#include <atomic>
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

namespace {
    struct CopyFailure {
        CopyFailure() = default;
        CopyFailure(const CopyFailure&) { throw std::runtime_error("payload copy failed"); }
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

TEST(event_bus, queued_delivery_owns_payloads_and_preserves_their_order) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    std::vector<int> received;
    std::vector<std::exception_ptr> errors;
    bus.register_listener("tick", [&](std::any value) { received.push_back(std::any_cast<int>(value)); },
        target.target(), [&](std::exception_ptr error) { errors.push_back(error); });
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

TEST(event_bus, unregister_discards_queued_callbacks_without_touching_the_old_target) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    int calls = 0;
    int errors = 0;
    const auto id = bus.register_listener("tick", [&](std::any) { ++calls; }, target.target(),
        [&](std::exception_ptr) { ++errors; });
    bus.dispatch("tick", 0);
    EXPECT_TRUE(bus.unregister_and_wait(id));
    target.drain();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(errors, 0);
}

TEST(event_bus, concurrent_close_discards_pending_delivery_without_waiting_for_running_callbacks) {
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
    bus.register_listener("queued", [&](std::any) { ++queued_calls; }, target.target(),
        [&](std::exception_ptr) { ++errors; });
    bus.dispatch("queued", 0);
    std::thread producer([&] { bus.dispatch("running", 0); });
    entered.get_future().wait();
    std::promise<void> close_start;
    auto may_close = close_start.get_future().share();
    auto first = std::async(std::launch::async, [&] { may_close.wait(); bus.close(); });
    auto second = std::async(std::launch::async, [&] { may_close.wait(); bus.close(); });
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

TEST(event_bus, removal_releases_callback_captures_outside_registry_and_listener_locks) {
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

TEST(event_bus, dropped_accepted_work_reports_cancellation_and_callback_errors_remain_observable) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    std::vector<std::exception_ptr> errors;
    bus.register_listener("tick", [](std::any) { throw std::runtime_error("callback failed"); }, target.target(),
        [&](std::exception_ptr error) { errors.push_back(error); });
    bus.dispatch("tick", 0);
    target.pending.clear();
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_THROW(std::rethrow_exception(errors[0]), std::future_error);
    bus.dispatch("tick", 0);
    target.drain();
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_THROW(std::rethrow_exception(errors[1]), std::runtime_error);
}

TEST(event_bus, delivery_rejection_reports_outside_the_enqueue_lock) {
    CE::SubSystems::EventBus bus;
    CE::SubSystems::EventBus::Registration id;
    int failures = 0;
    id = bus.register_listener("tick", [](std::any) {},
        [](CE::SubSystems::EventBus::Work) { return false; },
        [&](std::exception_ptr error) {
            ++failures;
            EXPECT_THROW(std::rethrow_exception(error), std::future_error);
            // Reporting may safely remove the listener which failed delivery.
            EXPECT_TRUE(bus.unregister_listener(id));
        });
    bus.dispatch("tick", 0);
    EXPECT_EQ(failures, 1);
}

TEST(event_bus, queued_delivery_requires_an_observable_error_sink) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    EXPECT_THROW(bus.register_listener("tick", [](std::any) {}, target.target()), CE::Exceptions::invalid_args);
}

TEST(event_bus, queued_payload_copy_failure_reports_the_original_error_once_and_allows_sink_reentry) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    CE::SubSystems::EventBus::Registration id;
    int failures = 0;
    int recovered = 0;
    bus.register_listener("recovery", [&](std::any) { ++recovered; });
    id = bus.register_listener("tick", [](std::any) { ADD_FAILURE() << "Uncopyable payload entered callback"; },
        target.target(), [&](std::exception_ptr error) {
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
        });
    const std::any payload(std::in_place_type<CopyFailure>);
    EXPECT_NO_THROW(bus.dispatch("tick", payload));
    EXPECT_EQ(failures, 1);
    EXPECT_EQ(recovered, 1);
    EXPECT_TRUE(target.pending.empty());
}

TEST(event_bus, a_throwing_delivery_target_reports_its_original_error_and_can_be_used_again) {
    CE::SubSystems::EventBus bus;
    QueuedDelivery target;
    bool reject = true;
    int failures = 0;
    int calls = 0;
    bus.register_listener("tick", [&](std::any) { ++calls; },
        [&](CE::SubSystems::EventBus::Work work) {
            if (reject)
                throw std::runtime_error("target unavailable");
            return target.target()(std::move(work));
        }, [&](std::exception_ptr error) {
            ++failures;
            EXPECT_THROW(std::rethrow_exception(error), std::runtime_error);
        });
    bus.dispatch("tick", 0);
    EXPECT_EQ(failures, 1);
    reject = false;
    bus.dispatch("tick", 0);
    target.drain();
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(failures, 1);
}

TEST(event_bus, a_worker_delivery_stream_preserves_callback_completion_order_on_a_parallel_pool) {
    CE::Engine::WorkerPool pool(3);
    auto group = pool.make_group();
    auto delivery = CE::Engine::worker_event_delivery(group);
    CE::SubSystems::EventBus bus;
    std::vector<int> received;
    std::atomic<int> errors{0};
    bus.register_listener("tick", [&](std::any value) { received.push_back(std::any_cast<int>(value)); },
        delivery, [&](std::exception_ptr) { ++errors; });
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

TEST(event_bus, copied_worker_targets_share_a_stream_across_listeners) {
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

TEST(event_bus, a_saved_worker_target_reports_rejection_after_its_pool_is_destroyed) {
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

TEST(event_bus, closing_a_worker_group_keeps_accepted_callbacks_and_rejects_new_delivery_with_reentrant_reporting) {
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
    bus.register_listener("accepted", [&](std::any value) {
        if (std::any_cast<int>(value) == 0) {
            entered.set_value();
            may_finish.wait();
        }
        received.push_back(std::any_cast<int>(value));
    }, delivery, unexpected);
    bus.dispatch("accepted", 0);
    EXPECT_EQ(started.wait_for(std::chrono::seconds{1}), std::future_status::ready);
    bus.dispatch("accepted", 1); // The same pump owns this queued callback.
    group.close();

    int errors = 0;
    int immediate = 0;
    CE::SubSystems::EventBus::Registration rejected;
    bus.register_listener("report", [&](std::any) { ++immediate; });
    rejected = bus.register_listener("rejected", [](std::any) { ADD_FAILURE() << "Closed group invoked a new callback"; },
        delivery, [&](std::exception_ptr error) {
            EXPECT_THROW(std::rethrow_exception(error), std::future_error);
            ++errors;
            bus.dispatch("report", 0);
            EXPECT_TRUE(bus.unregister_listener(rejected));
        });
    bus.dispatch("rejected", 2);
    EXPECT_EQ(errors, 1);
    EXPECT_EQ(immediate, 1);
    release.set_value();
    group.drain();
    EXPECT_EQ(received, (std::vector<int>{0, 1}));
    bus.close();
}

TEST(event_bus, independent_worker_streams_can_progress_while_one_callback_is_held) {
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
    bus.register_listener("held", [&](std::any) { entered.set_value(); may_finish.wait(); },
        CE::Engine::worker_event_delivery(group), unexpected);
    bus.register_listener("independent", [&](std::any) { independent.set_value(); },
        CE::Engine::worker_event_delivery(group), unexpected);
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
