#pragma once

#include "../../internals.h"
#include "../../interfaces/pool.h"
#include "../memory.h"

namespace CherylE{
    /* AssetPool
    ************
    Offers contiguous memory management for objects.

    Object Memory is tracked via three containers.
    1)A master record set which records all allocations.
    2)An open list which records all available sub-allocations
    3)A closed list which records all unavailable sub-allocations

    Services:
    * pre-allocate memory
    * get available memory
    * resize acquired memory
    * return memory
    * purge memory

    This header is partitioned into two parts.
    - Part 1: class definition, (ie. tldr interface)
    - Part 2: method implementations
    */
    // Part 1 - Definition
    template<class AssetType> //todo: "override" valid? this is the first implementation
    class AssetPool : public iPoolT<iAsset>{
        TYPENAMEAVAILABLE_STATIC
        static_assert(isderived(iAsset,AssetType),"In AssetPool<T>, T must be derived from iAsset.");
    protected:
        alloc allocate(const size_t &N);
    public:
        AssetPool(){
            type_size = sizeof(AssetType);
        }
        /*frees all memory*/
        virtual void purge() override;
        /*returns an array of size N*/
        virtual iAsset* getT(const size_t N, const fitType fit = fitType::worstFit) override;
    };

    // Part 2 - Implementation
    template<class A>
    alloc AssetPool<A>::allocate(const size_t N){
        void* p = Singleton<MemoryMgr>::get().get(N * type_size); //exception when N=0
        ce_uintptr ptr = (ce_uintptr)p;
        MasterRecord.emplace(ptr);
        m_total += bytes;
        m_free += bytes;
        return alloc{ptr,ptr,N,N};;
    }
    template<class A>
    void AssetPool<A>::purge(){
        for(auto p : MasterRecord){
            Singleton<MemoryMgr>::get().put(p);
        }
        MasterRecord.clear();
        OpenList.clear();
        ClosedList.clear();
        m_free = 0;
        m_used = 0;
        m_total = 0;
    }
    template<class A>
    iAsset* AssetPool<A>::getT(const size_t N, const fitType fit){
        A* p = get(N,fit);
        for(int i = 0; i < N; ++i){
            p[i] = A();
        }
        return p;
    }

}