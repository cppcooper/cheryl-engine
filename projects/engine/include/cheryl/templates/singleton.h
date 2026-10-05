#pragma once
#include <internals/exceptions.h>
#include <ctti/name.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

/* Singleton_CTS (Compile Time Safe)
 * For simple singleton creation.
 * You can keep inheriting these all you want, it will always be the same instance they point to.
 * Uses static_assert verifying constructibility - requires public constructor(s)
 * initialize(args...) accepts exactly one successful explicit initialization; repeated calls reject.
 * Configure argument-bearing instances on their owner thread before starting producers. get(args...)
 * remains a compatibility accessor: the first successful construction wins and later arguments are ignored.
 * get() constructs a default-constructible Type, otherwise it only retrieves a published instance.
 * A failed constructor publishes nothing and permits retry. Nonconstructing reads never wait for construction.
 * Construction/publication is synchronized; Type's later operations require its own thread-safety contract.
 * Returned pointers/references borrow process-lifetime storage, not ownership. Quiesce all callers before
 * static destruction; neither get_existing() nor a successful read pins an instance against teardown.
 */
template <class Type> class Singleton_CTS {
    struct Storage {
        std::once_flag construct_flag;
        std::unique_ptr<Type> owner;
        std::atomic<Type*> published{nullptr};

        ~Storage() { published.store(nullptr, std::memory_order_release); }
    };

    static Storage& storage() {
        static Storage value;
        return value;
    }

    template <typename... Args> static bool construct(Args&&... args) {
        static_assert(std::is_constructible_v<Type, Args...>, "A constructor doesn't exist for your Type in Singleton<Type>");
        auto& value = storage();
        bool initialized = false;
        std::call_once(value.construct_flag, [&]() {
            value.owner = std::make_unique<Type>(std::forward<Args>(args)...);
            value.published.store(value.owner.get(), std::memory_order_release);
            initialized = true;
        });
        return initialized;
    }

public:
    // Observe publication without creating or waiting. Null also means construction is still in progress.
    [[nodiscard]] static Type* get_existing() noexcept { return storage().published.load(std::memory_order_acquire); }

    template <typename... Args> static Type& initialize(Args&&... args) {
        if (!construct(std::forward<Args>(args)...)) {
            throw CE::Exceptions::bad_request(CE_HERE, std::format("Singleton<{}> is already initialized.", ctti::name_of<Type>()));
        }
        return *get_existing();
    }

    // Constructors must not recursively initialize/get this same singleton.
    template <typename... Args> static Type& get(Args&&... args) {
        if constexpr (std::is_constructible_v<Type, Args...>) {
            construct(std::forward<Args>(args)...);
        }
        if (auto* instance = get_existing()) {
            return *instance;
        }
        throw CE::Exceptions::failed_operation(CE_HERE, std::format("Singleton<{}> has not been initialized.", ctti::name_of<Type>()));
    }
};

/* Singleton_CTU (Compile Time Unsafe)
 * For true singleton creation. i.e. instantiation is private/protected
 * Type must friend Singleton_CTU<Type> so construction is accessible in this template's context.
 * Deletion by std::unique_ptr still requires an accessible destructor. The initialization,
 * publication, retry, borrowed-lifetime, and operation contracts are the same as Singleton_CTS.
 *
 * Example usage:
 * class Foo : public Singleton_CTU<Foo> {
 *   friend class Singleton_CTU<Foo>;
 *   Foo() = default; // private constructor, can't be instantiated
 * public:
 *   // interface
 * };
 */
template <class Type> class Singleton_CTU {
    struct Storage {
        std::once_flag construct_flag;
        std::unique_ptr<Type> owner;
        std::atomic<Type*> published{nullptr};

        ~Storage() { published.store(nullptr, std::memory_order_release); }
    };

    static Storage& storage() {
        static Storage value;
        return value;
    }

    // std::is_constructible/make_unique check access in their own context, not the befriended class.
    template <typename... Args> static constexpr bool can_construct = requires(Args&&... args) {
        new Type(std::forward<Args>(args)...);
    };

    template <typename... Args> static bool construct(Args&&... args) {
        static_assert(can_construct<Args...>, "A constructor doesn't exist for your Type in Singleton<Type>");
        auto& value = storage();
        bool initialized = false;
        std::call_once(value.construct_flag, [&]() {
            value.owner.reset(new Type(std::forward<Args>(args)...));
            value.published.store(value.owner.get(), std::memory_order_release);
            initialized = true;
        });
        return initialized;
    }

protected:
    Singleton_CTU() = default;

public:
    [[nodiscard]] static Type* get_existing() noexcept { return storage().published.load(std::memory_order_acquire); }

    template <typename... Args> static Type& initialize(Args&&... args) {
        if (!construct(std::forward<Args>(args)...)) {
            throw CE::Exceptions::bad_request(CE_HERE, std::format("Singleton<{}> is already initialized.", ctti::name_of<Type>()));
        }
        return *get_existing();
    }

    template <typename... Args> static Type& get(Args&&... args) {
        if constexpr (can_construct<Args...>) {
            construct(std::forward<Args>(args)...);
        }
        if (auto* instance = get_existing()) {
            return *instance;
        }
        throw CE::Exceptions::failed_operation(CE_HERE, std::format("Singleton<{}> has not been initialized.", ctti::name_of<Type>()));
    }
};
