#pragma once
#include "../internals.h"
#include "type-counter.h"

namespace CherylE{
    template<class T>
    class Factory : public Singleton<Factory<T>>{
        static_assert(isclass(T),"In Factory<T>, T must be a class.");
        //todo: type info required (T methods)
    protected:
        size_t typeID = 0;
        Factory(){
            static size_t count = 0;
            typeID = ++count;
            //TypeName();
            //maybe todo: register with higher power
        }
    public:
        T* create();
        void destroy(T* p);
        size_t get_typeID() const { return typeID; }

        template<typename... Args>
        void construct(T* p, Args... args){
            p = T{std::forward<Args>(args)...};
        }
    };

    template<class T>
    T* Factory<T>::create(){
        if(isderived(iAsset,T)){
            return Singleton<AssetPool>::get().get(1);
        } else {
            return Singleton<GenericPool>::get().get(1);
        }
    }

    template<class T>
    void Factory<T>::destroy(T* p){
        if(isderived(iAsset,T)){
            return Singleton<AssetPool>::get().put(p,1);
        } else {
            return Singleton<GenericPool>::get().put(p,1);
        }
    }

}