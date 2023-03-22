#pragma once
#include <internals.h>
#include <memory>

namespace CherylE{

    // Creates a singleton of Type
    template<class Type>
    class ConcreteSingleton {
    TYPENAMEAVAILABLE_STATIC
        //static_assert(hasmethod(T,TypeName),"In Singleton<T>, T must have the method TypeName, it needs to return a string.");
    protected:
        static std::shared_ptr<Type>& getImpl() {
            static std::shared_ptr<Type> instance;
            return instance;
        }
    public:
        static const std::shared_ptr<Type>& get() { return getImpl(); }

        template<typename... Args>
        explicit ConcreteSingleton(Args... args) {
            construct(std::forward<Args>(args)...);
        }

        // with variadic template args we forward construction responsibilities to the user, along with the compile time guarantee that constructors are correct
        template<typename... Args>
        static const std::shared_ptr<Type>& construct(Args... args) {
            auto &instance = getImpl();
            if (!instance) {
                instance = std::make_shared<Type>(std::forward<Args>(args)...);
            } else {
                throw bad_request(__CEFUNCTION__, __LINE__, "Instance already constructed.");
            }
            return instance;
        }
    };

    // Creates a singleton of Type, and prevents children from instantiating
    template<class Type>
    class SuperSingleton {
    TYPENAMEAVAILABLE_VIRTUAL
    private:
        // prevent derived classes from instantiating themselves
        SuperSingleton() = default;
    protected:
        static std::shared_ptr<Type>& getImpl() {
            static std::shared_ptr<Type> instance;
            return instance;
        }
    public:
        static const std::shared_ptr<Type>& get() { return getImpl(); }

        // with variadic template args we forward construction responsibilities to the user, along with the compile time guarantee that constructors are correct
        template<typename... Args>
        static const std::shared_ptr<Type>& construct(Args... args) {
            auto &instance = getImpl();
            if (!instance) {
                // conversion of derived type to super type should still work for shared_ptr
                instance = std::make_shared<Type>(std::forward<Args>(args)...);
            } else {
                throw bad_request(__CEFUNCTION__, __LINE__, "Instance already constructed.");
            }
            return instance;
        }
    };

    // Is a singleton derived from Type, and prevents children from instantiating
    template<class Type>
    class DerivedSingleton : public Type {
    TYPENAMEAVAILABLE_VIRTUAL
    private:
        using Singleton = DerivedSingleton<Type>;
        // prevent derived classes from instantiating themselves
        template<typename... Args>
        explicit DerivedSingleton(Args... args) : Type(std::forward<Args>(args)...) {}

        static std::shared_ptr<Singleton>& getImpl() {
            static std::shared_ptr<Singleton> instance;
            return instance;
        }
    public:
        static const std::shared_ptr<Singleton>& get() { return getImpl(); }

        // todo: this ctor is private, so a derived class can't instantiate.. because the derived constructor can't be invoked
        //  therefore, how can we construct a "DerivedType"
        // with variadic template args we forward construction responsibilities to the user, along with the compile time guarantee that constructors are correct
        template<class DerivedType, typename... Args>
        static const std::shared_ptr<Singleton>& construct(Args... args) {
            static_assert(isderived(Singleton, DerivedType), "DerivedType must in fact be a derived type of DerivedSingleton<Type>");
            auto &instance = getImpl();
            if (!instance) {
                // conversion of derived type to super type should still work for shared_ptr
                // the DerivedType ctor can forward to DerivedSingleton's ctor
                instance = std::make_shared<DerivedType>(std::forward<Args>(args)...);
            } else {
                throw bad_request(__CEFUNCTION__, __LINE__, "Instance already constructed.");
            }
            return instance;
        }
    };
}
