#pragma once
#include "../internals.h"

namespace CherylE{    
    template<class AssetType>
    class AssetFactory : public iFactory<iAsset,AssetFactory>{
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
}