#pragma once
#include "../internals.h"

namespace CherylE{    
    template<class AssetType>
    class AssetFactory :
        public iFactory<iAsset,AssetFactory>,
        public Singleton<AssetFactory<AssetType>>
    {
    protected:
        AssetFactory(){/*factory with tracker*/}
    public:
        iAsset* create(){
            AssetType* p2 = AssetPool<AssetType>.get(1);
            return static_cast<iAsset*>(p2);
        }
        void destroy(iAsset* p){
            AssetType* p2 = static_cast<AssetType*>(p);
            AssetPool<T>.put(p2);
        }
        template<typename... Args>
        void construct_impl(iAsset* p, Args... args){
            AssetType* p2 = static_cast<AssetType*>(p);
            *p2 = AssetType{std::forward<Args>(args)...};
        }
    };

    template<class T>
    class GenericFactory :
        public iFactory<void,GenericFactory>,
        public Singleton<GenericFactory<T>>
    {
    protected:
        static_assert(hasmethod(T,TypeName),"GenericFactory<T> requires T to implement the TypeName method to provide type safety.");
        static_assert(!isderived(iAsset,T),"Use AssetFactory for iAsset derived types.");
        GenericFactory(){/*register with tracker*/}
    public:
        //todo: implement methods
        void* create(){
            T* p = nullptr;
            return (void*)p;
        }
        void destroy(void* p){
            T* p2 = (T*)p;
            //if p2->TypeName() == validT.TypeName()
            //else throw bad_request exception
        }
        template<typename... Args>
        void construct_impl(void* p, Args... args){
            static T validT;
            T* p2 = (T*)p;
            //if p2->TypeName() == validT.TypeName()
            *p2 = T{std::forward<Args>(args)...};
            //else throw bad_request exception
        }
    };
}