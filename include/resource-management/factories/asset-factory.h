#pragma once
#include "../../internals.h"
#include "type-counter.h"

namespace CherylE{
    template<class AssetType>
    class AssetFactory : public iFactory<iAsset,AssetFactory>, public Singleton<AssetFactory<AssetType>>{
    protected:
        AssetFactory(){
            typeID = typeCounter(true);
            TypeName();
            /*register with tracker*/
        }
    public:
        iAsset* create(){
            AssetType* p2 = AssetPool<AssetType>.get(1);
            return static_cast<iAsset*>(p2);
        }
        void destroy(iAsset* p){
            AssetType* p2 = static_cast<AssetType*>(p);
            AssetPool<T>.put(p2);
        }
        //protected?
        template<typename... Args>
        void construct_impl(iAsset* p, Args... args){
            AssetType* p2 = static_cast<AssetType*>(p);
            *p2 = AssetType{std::forward<Args>(args)...};
        }
        /*performs a runtime type-safe downcast without compiler RTTI
            not part of the factory interface, will probably never be used
        */
        AssetType* dynamic_downcast(iAsset* p){
            if(p->get_typeID() == typeID){
                return static_cast<AssetType*>(p);
            }
            return nullptr;
        }
        const char* TypeName(){
            static AssetType type;
            return type.TypeName();
        }
    };
}