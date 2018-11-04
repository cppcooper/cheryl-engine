#pragma once
#include "../internals.h"

namespace CherylE{
    template<class T, typename... Args>
    static void construct(T& instance, Args... args){
        instance = T {std::forward<Args>(args)...};
    }

    template<class T>
    class Singleton{
    public:
        static_assert(hasmethod(T,TypeName),"In Singleton<T>, T must have the method TypeName, it needs to return a string.");
        static T& get(){
            static T instance;
            return instance;
        }

        template<typename... Args>
        static void construct_once(Args... args){
            static bool called_once = false;
            if(!called_once){
                called_once = true;
                construct(get(), args...); //should deduce types
            } else {
                char buffer[128];
                snprintf(buffer,128,"Already constructed Singleton<%s>",get().TypeName());
                throw bad_request(__FUNCTION__,__LINE__,buffer);
            }
        }
    };
}