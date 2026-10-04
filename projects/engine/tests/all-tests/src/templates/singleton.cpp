#include <gtest/gtest.h>
#include <templates/singleton.h>

#include <array>
#include <barrier>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
    struct Configured : Singleton_CTS<Configured> {
        int value;

        explicit Configured(int value)
        : value(value) {}
    };

    struct Racing : Singleton_CTS<Racing> {
        int value;

        explicit Racing(int value)
        : value(value) {}
    };

    struct Retried : Singleton_CTS<Retried> {
        explicit Retried(bool fail) {
            if (fail)
                throw std::runtime_error("construction rejected");
        }
    };

    struct Pending : Singleton_CTS<Pending> {
        int value = 17;

        Pending(std::promise<void>& entered, std::shared_future<void> release) {
            entered.set_value();
            release.wait();
        }
    };

    class Private : public Singleton_CTU<Private> {
        friend class Singleton_CTU<Private>;
        int value_;

        explicit Private(int value)
        : value_(value) {}

    public:
        [[nodiscard]] int value() const { return value_; }
    };

    class DefaultPrivate : public Singleton_CTU<DefaultPrivate> {
        friend class Singleton_CTU<DefaultPrivate>;
        DefaultPrivate() = default;

    public:
        ~DefaultPrivate() = default;
    };

    struct MoveOnly : Singleton_CTS<MoveOnly> {
        std::unique_ptr<int> value;

        explicit MoveOnly(std::unique_ptr<int> value)
        : value(std::move(value)) {}
    };
}

TEST(singleton, explicit_init) {
    EXPECT_EQ(Configured::get_existing(), nullptr);
    EXPECT_THROW(Configured::get(), CE::Exceptions::failed_operation);
    auto& instance = Configured::initialize(23);
    EXPECT_EQ(instance.value, 23);
    EXPECT_EQ(&Configured::get(), &instance);
    EXPECT_EQ(Configured::get_existing(), &instance);
    EXPECT_THROW(Configured::initialize(41), CE::Exceptions::bad_request);
    // Existing get(args...) users retain first-successful-construction behavior.
    EXPECT_EQ(Configured::get(41).value, 23);
}

TEST(singleton, competing_init) {
    std::barrier start(3);
    std::array<bool, 2> accepted{};
    std::array<std::exception_ptr, 2> failures{};
    const auto initialize = [&](std::size_t index) {
        start.arrive_and_wait();
        try {
            Racing::initialize(static_cast<int>(index + 10));
            accepted[index] = true;
        } catch (...) {
            failures[index] = std::current_exception();
        }
    };
    std::thread first(initialize, 0);
    std::thread second(initialize, 1);
    start.arrive_and_wait();
    first.join();
    second.join();
    ASSERT_NE(accepted[0], accepted[1]);
    const auto winner = accepted[0] ? 0u : 1u;
    EXPECT_EQ(Racing::get().value, winner + 10);
    ASSERT_NE(failures[1 - winner], nullptr);
    EXPECT_THROW(std::rethrow_exception(failures[1 - winner]), CE::Exceptions::bad_request);
}

TEST(singleton, failed_init) {
    EXPECT_THROW(Retried::initialize(true), std::runtime_error);
    EXPECT_EQ(Retried::get_existing(), nullptr);
    EXPECT_THROW(Retried::get(), CE::Exceptions::failed_operation);
    auto& instance = Retried::initialize(false);
    EXPECT_EQ(&Retried::get(), &instance);
    EXPECT_THROW(Retried::initialize(false), CE::Exceptions::bad_request);
}

TEST(singleton, pending_init) {
    std::promise<void> entered;
    auto entry = entered.get_future();
    std::promise<void> release;
    auto gate = release.get_future().share();
    auto initialized = std::async(std::launch::async, [&] { return &Pending::initialize(entered, gate); });
    entry.wait();
    EXPECT_EQ(Pending::get_existing(), nullptr);
    EXPECT_THROW(Pending::get(), CE::Exceptions::failed_operation);
    release.set_value();
    auto* instance = initialized.get();
    EXPECT_EQ(Pending::get_existing(), instance);
    EXPECT_EQ(Pending::get().value, 17);
}

TEST(singleton, private_ctor) {
    EXPECT_THROW(Private::get(), CE::Exceptions::failed_operation);
    auto& instance = Private::initialize(29);
    EXPECT_EQ(Private::get().value(), 29);
    EXPECT_EQ(Private::get_existing(), &instance);
    EXPECT_THROW(Private::initialize(31), CE::Exceptions::bad_request);
    EXPECT_EQ(&DefaultPrivate::get(), &DefaultPrivate::get());
}

TEST(singleton, move_only) {
    auto& instance = MoveOnly::initialize(std::make_unique<int>(37));
    EXPECT_EQ(*instance.value, 37);
    EXPECT_EQ(&MoveOnly::get(), &instance);
}
