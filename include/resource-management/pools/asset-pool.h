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
    class AssetPool : public iPoolT<iAsset,AssetPool>{
        TYPENAMEAVAILABLE_STATIC
    protected:
        static_assert(isderived(iAsset,AssetType),"In AssetPool<T>, T must be derived from iAsset.");
        alloc allocate(size_t &N);
        void moveto_open(closed_iter iter, size_t N);
        virtual closed_iter find_closed(iAsset* p) override;
    public:
        /*frees all memory*/
        virtual void purge() override;
        /*allocates M blocks of N bytes/objects [order of arguments: N,M]*/
        virtual void pre_allocate(size_t N, size_t blocks) override;
        /*returns an array of size N*/
        virtual iAsset* get(size_t N, fitType fit = fitType::worstFit) override;
        /*returns size of array*/
        virtual size_t size(iAsset* p) override;
        /*attempts to resize p to N elements, if allowed will reallocate if no other option is available*/
        virtual bool resize(iAsset* &p, size_t N, bool allow_realloc = false) override;
        /*returns all the elements from p to the end of the array p belongs to*/
        virtual void put(iAsset* p) override;
        /*returns all the elements from p to p+N of the array p belongs to*/
        virtual void put(iAsset* p, size_t N) override;

    };
    /*template<typename T>
    class GenericPool : public iPool{ //: public MemoryMgr??
        TYPENAMEAVAILABLE_STATIC
    protected:
        static_assert(hasmethod(T,TypeName),"In GenericPool<T>, T must implement the TypeName for type safety checks");
        static_assert(!isderived(iAsset,T),"Use AssetPool for iAsset derived types.");
    };*/


    // Part 2 - Implementation
    template<class A>
    alloc AssetPool<A>::allocate(size_t N){
        void* p = Singleton<MemoryMgr>::get().get(N * sizeof(A)); //exception when N=0
        alloc a{p,p,N,N};
        MasterRecord.emplace(p);
        OpenList.emplace(N,a);
    }
    template<class A>
    void AssetPool<A>::moveto_open(closed_iter iter, size_t N){
        alloc a = iter->second;
        size_t remainder = 0;
        if(N <= a.head_size){
            remainder = a.head_size - N;
            a.head_size = N;
            m_used -= N;
            m_free += N;
        } else {
            throw invalid_args(__FUNCTION__, __LINE__, "The objects returned must be less than or equal to the number allocated.");
        }
        ClosedList.erase(iter);
        OpenList.emplace(N,a);
        if(remainder>0){
            a.head += a.head_size * sizeof(A);
            a.head_size = remainder;
            ClosedList.emplace(a.master,a);
        }
    }
    template<class A>
    closed_iter AssetPool<A>::find_closed(iAsset* p){
        /* Get first pair with a key not less than p
        -p is a masterptr
            search will land directly on the right pair
        -p is not a master pointer
            -p is not the head of an allocation
                must find the head
            -p is the head of an allocation
                must match p to pair.second.head
        */
        if(!ClosedList.empty()){
            auto iter = ClosedList.find(p);
            if(iter!=ClosedList.end()){
                //we found an exact match
                return iter;
            }
            iter = ClosedList.lower_bound(p);
            if(iter!=ClosedList.begin()){
                //we found something not less than what we're looking for
                iter--;
                void* mptr = iter->first; //p should mptr < p < mptr+master_size
                if(mptr < p && p < mptr+(iter->second.master_size * sizeof(A))) {
                    //we just need to do a linear search now to find the right sub-allocation
                    while(true){
                        if(mptr != iter->first){
                            //we've gone too far
                            return ClosedList.end();
                        }
                        alloc a = iter->second;
                        if(p == a.head){
                            return iter;
                        } else if (a.head < p && p < a.head+(a.head_size * sizeof(A))){
                            //we found the sub-allocation that p is a sub-allocation of
                            return iter;
                        }
                        if(iter==ClosedList.begin()){
                            break;
                        }
                        iter--;
                    }
                }
            }
        }
        return ClosedList.end();
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
    void AssetPool<A>::pre_allocate(size_t N, size_t blocks){
        if(blocks < 1){
            throw invalid_args(__FUNCTION__, __LINE__, "The number of allocated blocks cannot be less than 1.");
        }
        for(int i = 0; i < blocks; ++i){
            size_t bytes = N * sizeof(A);
            alloc a = allocate(bytes);
            OpenList.emplace(N,a);
        }
    }
    template<class A>
    iAsset* AssetPool<A>::get(size_t N, fitType fit){
        if (N < 1){
            throw invalid_args(__FUNCTION__, __LINE__, "The number of objects must be at least 1.");
        }
        alloc a;
        //get an allocation {best fit}{worst fit}
        if (fit == fitType::bestFit){
            auto iter = OpenList.lower_bound(N); //best fit
            if(iter!=OpenList.end()){
                a = iter->second;
                OpenList.erase(iter);
            }else{
                a = allocate(N);
            }
        } else if (fit == fitType::worstFit){
            auto iter = OpenList.rbegin(); //worst fit
            if(iter!=OpenList.rend()){
                a = iter->second;
                OpenList.erase(iter);
            }else{
                a = allocate(N);
            }
        }
        size_t remainder = N - a.head_size;
        a.head_size = N;
        m_used += N;
        m_free -= N;
        ClosedList.emplace(a.master,a);
        if(remainder>0){
            a.head += a.head_size * sizeof(A);
            a.head_size = remainder;
            OpenList.emplace(a.master,a);
        }
    }
    template<class A>
    size_t AssetPool<A>::size(iAsset* p){
        //todo:
        auto iter = find_closed(p);
        if(iter!=ClosedList.end()){
            alloc a = iter->second;
            if(p==a.head){
                return a.head_size;
            }
            //check that p is in range
            if(a.head < p && p < (a.head+(a.head_size * sizeof(A)))){
                size_t offset = p - a.head;
                if(offset % sizeof(A) != 0){
                    throw //mis-alignment
                }
                return a.head_size - (offset/sizeof(A));
            }
        }
        return 0;
    }

}