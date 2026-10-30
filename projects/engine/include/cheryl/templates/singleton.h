#pragma once
#include <internals/exceptions.h>
#include <ctti/name.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

namespace CE::State {

    /** CE::State::Singleton (Compile Time Safe)
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
    template <class Type> class Singleton {
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
        [[nodiscard]] static Type* get_existing() noexcept { return storage().published.load(std::memory_order_acquire); }

        template <typename... Args> static Type& initialize(Args&&... args) {
            if (!construct(std::forward<Args>(args)...)) {
                throw ::CE::Exceptions::bad_request(
                    CE_HERE, std::string("Singleton<").append(ctti::name_of<Type>()).append("> is already initialized.")
                );
            }
            return *get_existing();
        }

        template <typename... Args> static Type& get(Args&&... args) {
            if constexpr (std::is_constructible_v<Type, Args...>) {
                construct(std::forward<Args>(args)...);
            }
            if (auto* instance = get_existing()) {
                return *instance;
            }
            throw ::CE::Exceptions::failed_operation(
                CE_HERE, std::string("Singleton<").append(ctti::name_of<Type>()).append("> has not been initialized.")
            );
        }
    };

    /** CE::State::FriendSingleton (Compile Time Unsafe)
 * For true singleton creation. i.e. instantiation is private/protected
 * Type must friend CE::State::FriendSingleton<Type> so construction is accessible in this template's context.
 * Deletion by std::unique_ptr still requires an accessible destructor. The initialization,
 * publication, retry, borrowed-lifetime, and operation contracts are the same as CE::State::Singleton.
 *
 * Example usage:
 * class Foo : public CE::State::FriendSingleton<Foo> {
 *   friend class CE::State::FriendSingleton<Foo>;
 *   Foo() = default; // private constructor, can't be instantiated
 * public:
 *   // interface
 * };
 */
    template <class Type> class FriendSingleton {
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
        template <typename... Args>
        static constexpr bool can_construct = requires(Args&&... args) { new Type(std::forward<Args>(args)...); };

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
        FriendSingleton() = default;

    public:
        [[nodiscard]] static Type* get_existing() noexcept { return storage().published.load(std::memory_order_acquire); }

        template <typename... Args> static Type& initialize(Args&&... args) {
            if (!construct(std::forward<Args>(args)...)) {
                throw ::CE::Exceptions::bad_request(
                    CE_HERE, std::string("Singleton<").append(ctti::name_of<Type>()).append("> is already initialized.")
                );
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
            throw ::CE::Exceptions::failed_operation(
                CE_HERE, std::string("Singleton<").append(ctti::name_of<Type>()).append("> has not been initialized.")
            );
        }
    };

}

/** @fn template<class Type> Type* CE::State::Singleton<Type>::get_existing() noexcept
 * @brief Observes a published instance without constructing or waiting.
 * @return Borrowed pointer or null when no instance is published, including construction in progress.
 * Reads are synchronized; the pointer does not pin storage against process teardown.
 * Later operations obey Type's own thread contract. Quiesce users before static destruction.
 */

/** @fn template<class Type> template<typename... Args> Type& CE::State::Singleton<Type>::initialize(Args&&... args)
 * @brief Performs exactly one successful explicit initialization.
 * @tparam Args Constructor argument types; must permit construction in this template's access context.
 * @param args Perfect-forwarded only to the winning construction.
 * @return Borrowed process-lifetime instance, not an owning reference.
 * @throws CE::Exceptions::bad_request When another successful construction already won.
 * Constructor failures publish nothing and allow retry. Concurrent construction
 * is serialized; constructors must not recursively initialize/get the same singleton.
 */

/** @fn template<class Type> template<typename... Args> Type& CE::State::Singleton<Type>::get(Args&&... args)
 * @brief Retrieves the instance, constructing it once when these arguments allow it.
 * @tparam Args Candidate constructor argument types.
 * @param args Forwarded on the first successful constructible call; later argument values are ignored.
 * @return Borrowed process-lifetime instance. Access does not establish Type's operation thread safety.
 * @throws CE::Exceptions::failed_operation When no instance is published and construction is unavailable.
 * A default-constructible Type can be constructed by get(); otherwise an argument-free
 * get only observes publication and need not wait for construction in progress.
 * Constructor failures propagate without publication and permit retry. Recursive
 * construction of this same singleton is unsupported.
 * @see [Singleton contracts](cheryl-guide:runtime/utility-contracts)
 */

/** @fn template<class Type> Type* CE::State::FriendSingleton<Type>::get_existing() noexcept
 * @brief Observes a published instance without constructing or waiting.
 * @return Borrowed pointer or null when no instance is published, including construction in progress.
 * Reads are synchronized; the pointer does not pin storage against process teardown.
 * Later operations obey Type's own thread contract. Quiesce users before static destruction.
 */

/** @fn template<class Type> template<typename... Args> Type& CE::State::FriendSingleton<Type>::initialize(Args&&... args)
 * @brief Performs exactly one successful explicit initialization.
 * @tparam Args Constructor argument types; must permit construction in this template's access context.
 * @param args Perfect-forwarded only to the winning construction.
 * @return Borrowed process-lifetime instance, not an owning reference.
 * @throws CE::Exceptions::bad_request When another successful construction already won.
 * Constructor failures publish nothing and allow retry. Concurrent construction
 * is serialized; constructors must not recursively initialize/get the same singleton.
 */

/** @fn template<class Type> template<typename... Args> Type& CE::State::FriendSingleton<Type>::get(Args&&... args)
 * @brief Retrieves the instance, constructing it once when these arguments allow it.
 * @tparam Args Candidate constructor argument types.
 * @param args Forwarded on the first successful constructible call; later argument values are ignored.
 * @return Borrowed process-lifetime instance. Access does not establish Type's operation thread safety.
 * @throws CE::Exceptions::failed_operation When no instance is published and construction is unavailable.
 * A default-constructible Type can be constructed by get(); otherwise an argument-free
 * get only observes publication and need not wait for construction in progress.
 * Constructor failures propagate without publication and permit retry. Recursive
 * construction of this same singleton is unsupported.
 * @see [Singleton contracts](cheryl-guide:runtime/utility-contracts)
 */

/** @fn template<class Type> CE::State::FriendSingleton<Type>::FriendSingleton()
 * @brief Allows a derived singleton wrapper to construct its base without creating Type.
 * Type must befriend FriendSingleton<Type> for inaccessible construction.
 * The default unique_ptr deleter still requires an accessible Type destructor.
 * This protected constructor performs no publication or allocation.
 */
