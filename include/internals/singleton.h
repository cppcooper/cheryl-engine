#pragma once
#include "../internals.h"

namespace CherylE{
    template<class T>
    class Singleton{
        TYPENAMEAVAILABLE_STATIC //this and the next line are a gonna cause a conflict
        static_assert(hasmethod(T,TypeName),"In Singleton<T>, T must have the method TypeName, it needs to return a string.");
    public:
        static T& get(){
            static T instance;
            return instance;
        }

        template<typename... Args>
        static void construct_singleton_once(Args... args){
            static bool called_once = false;
            if(!called_once){
                called_once = true;
                get() = T {std::forward<Args>(args)...};
            } else {
                char buffer[128];
                snprintf(buffer,128,"Already constructed %s",TypeName());
                throw bad_request(__FUNCTION__,__LINE__,buffer);
            }
        }
    };
}