#pragma once
#include "../internals.h"
#include <utility>

namespace CherylE{
    template<class T, typename... Args>
    static void construct(T& instance, Args... args){
        instance = T {std::forward<Args>(args)...};
    }

    template<class T>
    class Singleton{
    private:
        static T instance;
    public:
        static T& get(){
            return instance;
        }

        template<typename... Args>
        static void construct_once(Args... args){
            static bool called_once = false;
            if(!called_once){
                called_once = true;
                construct(instance, args...); //should deduce types
            } else {
                print_already_constructed_error(instance.TypeName());
            }
        }
    };

    template<class T>
    T Singleton<T>::instance;
}