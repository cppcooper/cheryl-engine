#pragma once
#include "../internals.h"

namespace CherylE{
    static size_t typeCounter(){
        static size_t count = 0;
        return ++count;
    }

    template<class AssetType>
    class AssetFactory :
        public iFactory<iAsset,AssetFactory>,
        public Singleton<AssetFactory<AssetType>>
    {
        protected:
            AssetFactory(){
                typeID = typeCounter();
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

    template<class T>
    class GenericFactory :
        public iFactory<void,GenericFactory>,
        public Singleton<GenericFactory<T>>
    {
        protected:
            static_assert(isclass(typeinterface),"In GenericFactory<T>, T must be a class.");
            static_assert(!isderived(iAsset,T),"Use AssetFactory for iAsset derived types.");
            static_assert(hasmethod(T,TypeName),"In GenericFactory<T>, T must implement the method TypeName");
            static_assert(hasmethod(T,get_typeID),"In GenericFactory<T>, T must implement the method typeID");
            GenericFactory(){
                typeID = typeCounter();
                TypeName();
                /*register with tracker*/
            }
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
            /*performs a runtime type-safe downcast without compiler RTTI
                not part of the factory interface, will probably never be used
            */
            T* dynamic_downcast(void* p){
                if(p->get_typeID() == typeID){
                    return static_cast<T*>(p);
                }
                return nullptr;
            }
            const char* TypeName(){
                static T type;
                return type.TypeName();
            }
    };
}