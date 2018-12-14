#pragma once
#include "../../internals.h"
#include "type-counter.h"

namespace CherylE{
    template<class T>
    class GenericFactory : public iFactory<void,GenericFactory>, public Singleton<GenericFactory<T>>{
        static_assert(isclass(typeinterface),"In GenericFactory<T>, T must be a class.");
        static_assert(!isderived(iAsset,T),"Use AssetFactory for iAsset derived types.");
        static_assert(hasmethod(T,TypeName),"In GenericFactory<T>, T must implement the method TypeName");
        static_assert(hasmethod(T,get_typeID),"In GenericFactory<T>, T must implement the method typeID");
    protected:
        GenericFactory(){
            typeID = typeCounter(true);
            TypeName();
            //todo: register with higher power
        }
    public:
    };
}