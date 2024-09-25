#pragma once
#include <internals/exceptions.h>
#include <ctti/detailed_nameof.hpp>
#include <memory>
#include <mutex>

/* Singleton_CTS (Compile Time Safe)
 * For simple singleton creation.
 * You can keep inheriting these all you want, it will always be the same instance they point to.
 * Uses static_assert verifying constructibility - requires public constructor(s)
 */
template<class Type>
class Singleton_CTS {
    static std::once_flag& construct_flag() {
        static std::once_flag flag;
        return flag;
    };
    static std::unique_ptr<Type>& get_impl() {
        static std::unique_ptr<Type> instance;
        return instance;
    }
    template<typename... Args>
    static void construct(Args... args) {
        static_assert(std::is_constructible_v<Type, Args...>, "A constructor doesn't exist for your Type in Singleton<Type>");
        std::call_once(construct_flag(), [&](){ get_impl() = std::make_unique<Type>(std::forward<Args>(args)...);});
    }
public:
    template<typename... Args>
    static Type& get(Args... args) {
        if constexpr (std::is_constructible_v<Type, Args...>) {
            construct(std::forward<Args>(args)...);
        }
        auto& up = get_impl();
        if (!up) {
            const std::string info = std::format("Construction of Singleton<{}> failed.", ctti::detailed_nameof<Type>().full_name().str());
            throw CE::Exceptions::failed_operation(CE_HERE, info.c_str());
        }
        return *up;
    }
};

/* Singleton_CTU (Compile Time Unsafe)
 * Access Safe // Inheritance Safe
 * You can be sure that your singleton isn't being reused in new classes.
 *
 * For true singleton creation. i.e. instantiation is private/protected
 *
 * Example usage:
 * class Foo : public Singleton_CTU<Foo> {
 *   friend class Singleton_CTU<Foo>;
 *   Foo() = default; // private constructor, can't be instantiated
 * public:
 *   // interface
 * };
 */
template<class Type>
class Singleton_CTU {
    static std::once_flag& construct_flag() {
        static std::once_flag flag;
        return flag;
    };
    static std::unique_ptr<Type>& get_impl() {
        static std::unique_ptr<Type> instance;
        return instance;
    }
    template<typename... Args>
    static void construct(Args... args) {
        static_assert(std::is_constructible_v<Type, Args...>, "A constructor doesn't exist for your Type in Singleton<Type>");
        std::call_once(construct_flag(), [&](){ get_impl() = std::make_unique<Type>(std::forward<Args>(args)...);});
    }
protected:
    Singleton_CTU() = default;
public:
    template<typename... Args>
    static Type& get(Args... args) {
        if constexpr (std::is_constructible_v<Type, Args...>) {
            construct(std::forward<Args>(args)...);
        }
        auto& up = get_impl();
        if (!up) {
            const std::string info = std::format("Construction of Singleton<{}> failed.", ctti::detailed_nameof<Type>().full_name().str());
            throw CE::Exceptions::failed_operation(CE_HERE, info.c_str());
        }
        return *up;
    }
};
