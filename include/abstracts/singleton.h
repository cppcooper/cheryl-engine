#pragma once
#include <internals.h>
#include <memory>

namespace CherylE{

    /*
     * These classes are intended to quickly create singletons in varying situations.
     * They each use variadic template args to forward construction responsibilities
     * to the user. This comes with a compile-time guarantee that constructors are correct.
     */

    // Creates a singleton of Type
    template<class Type>
    class ConcreteSingleton {
    TYPENAMEAVAILABLE_STATIC
        //static_assert(hasmethod(T,TypeName),"In Singleton<T>, T must have the method TypeName, it needs to return a string.");
    protected:
        // the user can't be allowed to modify the pointer, but this implementation requires that ability
        static std::shared_ptr<Type>& getImpl();
    public:
        // acquire the singleton object
        static Type& get();

        // construct the singleton object once, and before attempting to use it
        template<typename... Args>
        static const std::shared_ptr<Type>& construct(Args... args);
    };

    // Creates a singleton of Type, and prevents children from instantiating
    template<class Type>
    class SuperSingleton {
    TYPENAMEAVAILABLE_VIRTUAL
    private:
        // prevent derived classes from instantiating themselves
        SuperSingleton() = default;
    protected:
        static std::shared_ptr<Type>& getImpl();
    public:
        static const std::shared_ptr<Type>& get();

        // with variadic template args we forward construction responsibilities to the user, along with the compile time guarantee that constructors are correct
        template<typename... Args>
        static const std::shared_ptr<Type>& construct(Args... args);
    };

    // Similar to SuperSingleton, but is derived from Type instead of encapsulating it
    template<class Type>
    class SuperSingletonDerived : public Type {
    TYPENAMEAVAILABLE_VIRTUAL
    private:
        using Singleton = SuperSingletonDerived<Type>;
        // prevent derived classes from instantiating themselves
        template<typename... Args>
        explicit SuperSingletonDerived(Args... args) : Type(std::forward<Args>(args)...) {}

        static std::shared_ptr<Singleton>& getImpl();
    public:
        static const std::shared_ptr<Singleton>& get() { return getImpl(); }

        // todo: this ctor is private, so a derived class can't instantiate.. because the derived constructor can't be invoked
        //  therefore, how can we construct a "DerivedType"
        // with variadic template args we forward construction responsibilities to the user, along with the compile time guarantee that constructors are correct
        template<class DerivedType, typename... Args>
        static const std::shared_ptr<Singleton>& construct(Args... args);
    };

    // Methods
    /////////////


    ///////////////////////
    /// Concrete Singleton

    template<class Type>
    std::shared_ptr<Type> &ConcreteSingleton<Type>::getImpl() {
        static std::shared_ptr<Type> instance;
        return instance;
    }

    template<class Type>
    Type& ConcreteSingleton<Type>::get() {
        auto &instance = getImpl();
        if (!instance) {
            if constexpr (std::is_default_constructible_v<Type>) {
                instance = std::make_shared<Type>();
            } else {
                if constexpr (hasmethod(Type, TypeName)) {
                    throw failed_operation(_CE_HERE, exception_msg("The singleton object '%s' has not yet been constructed", Type::TypeName()));
                } else {
                    throw failed_operation(_CE_HERE, "The singleton object has not yet been constructed");
                }
            }
        }
        return *instance;
    }

    template<class Type>
    template<typename... Args>
    const std::shared_ptr<Type> &ConcreteSingleton<Type>::construct(Args... args) {
        auto &instance = getImpl();
        if (!instance) {
            instance = std::make_shared<Type>(std::forward<Args>(args)...);
        } else {
            throw bad_request(_CE_HERE, "Instance already constructed.");
        }
        return instance;
    }

    ////////////////////
    /// Super Singleton

    template<class Type>
    std::shared_ptr<Type> &SuperSingleton<Type>::getImpl() {
        static std::shared_ptr<Type> instance;
        return instance;
    }

    template<class Type>
    const std::shared_ptr<Type> &SuperSingleton<Type>::get() { return getImpl(); }

    template<class Type>
    template<typename... Args>
    const std::shared_ptr<Type> &SuperSingleton<Type>::construct(Args... args) {
        auto &instance = getImpl();
        if (!instance) {
            // conversion of derived type to super type should still work for shared_ptr
            instance = std::make_shared<Type>(std::forward<Args>(args)...);
        } else {
            throw bad_request(_CE_HERE, "Instance already constructed.");
        }
        return instance;
    }

    //////////////////////
    /// Derived Singleton

    template<class Type>
    std::shared_ptr<SuperSingletonDerived<Type>> &SuperSingletonDerived<Type>::getImpl() {
        static std::shared_ptr<Singleton> instance;
        return instance;
    }

    template<class Type>
    template<class DerivedType, typename... Args>
    const std::shared_ptr<SuperSingletonDerived<Type>> &SuperSingletonDerived<Type>::construct(Args... args) {
        static_assert(isderived(Singleton, DerivedType), "DerivedType must in fact be a derived type of DerivedSingleton<Type>");
        auto &instance = getImpl();
        if (!instance) {
            // conversion of derived type to super type should still work for shared_ptr
            // the DerivedType ctor can forward to DerivedSingleton's ctor
            instance = std::make_shared<DerivedType>(std::forward<Args>(args)...);
        } else {
            throw bad_request(_CE_HERE, "Instance already constructed.");
        }
        return instance;
    }

}
