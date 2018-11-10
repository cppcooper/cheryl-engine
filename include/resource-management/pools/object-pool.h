#pragma once

namespace CherylE{
    template<class derived, class base>
    class ObjectPool : public iPoolT<base>{ //pretty sure iPoolT<...> doesn't need the pool type
        TYPENAMEAVAILABLE_STATIC
    protected:
        static_assert(isderived(base,derived),"In ObjectPool<D,B>, D must be derived from B.");
        alloc allocate(size_t &N);
        void moveto_open(closed_iter iter, size_t N);
        virtual closed_iter find_closed(iAsset* p) override;
    public:
        /*frees all memory*/
        virtual void purge() override;
        /*allocates M blocks of N bytes/objects [order of arguments: N,M]*/
        virtual void pre_allocate(size_t N, size_t blocks) override;
        /*returns an array of size N*/
        virtual base* get(size_t N, fitType fit = fitType::worstFit) override;
        /*returns size of array*/
        virtual size_t size(base* p) override;
        /*attempts to resize p to N elements, if allowed will reallocate if no other option is available*/
        virtual bool resize(base* &p, size_t N, bool allow_realloc = false) override;
        /*returns all the elements from p to the end of the array p belongs to*/
        virtual void put(base* p) override;
        /*returns all the elements from p to p+N of the array p belongs to*/
        virtual void put(base* p, size_t N) override;
    };

    // Part 2 - Implementation
    template<class D, class B>
    alloc ObjectPool<D,B>::allocate(size_t N){
        void* p = Singleton<MemoryMgr>::get().get(N * sizeof(D)); //exception when N=0
        alloc a{p,p,N,N};
        MasterRecord.emplace(p);
        OpenList.emplace(N,a);
    }
    template<class D, class B>
    void ObjectPool<D,B>::moveto_open(closed_iter iter, size_t N){
        /* Method must look for touching allocations
        * On both sides.
        */
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
            a.head += a.head_size * sizeof(D);
            a.head_size = remainder;
            ClosedList.emplace(a.master,a);
        }
    }
    template<class D, class B>
    closed_iter ObjectPool<D,B>::find_closed(B* p){
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
                if(mptr < p && p < mptr+(iter->second.master_size * sizeof(D))) {
                    //we just need to do a linear search now to find the right sub-allocation
                    while(true){
                        if(mptr != iter->first){
                            //we've gone too far
                            return ClosedList.end();
                        }
                        alloc a = iter->second;
                        if(p == a.head){
                            return iter;
                        } else if (a.head < p && p < a.head+(a.head_size * sizeof(D))){
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
    template<class D, class B>
    void ObjectPool<D,B>::purge(){
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
    template<class D, class B>
    void ObjectPool<D,B>::pre_allocate(size_t N, size_t blocks){
        if(blocks < 1){
            throw invalid_args(__FUNCTION__, __LINE__, "The number of allocated blocks cannot be less than 1.");
        }
        for(int i = 0; i < blocks; ++i){
            size_t bytes = N * sizeof(D);
            alloc a = allocate(bytes);
            OpenList.emplace(N,a);
        }
    }
    template<class D, class B>
    B* ObjectPool<D,B>::get(size_t N, fitType fit){
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
            a.head += a.head_size * sizeof(D);
            a.head_size = remainder;
            OpenList.emplace(a.master,a);
        }
    }
    template<class D, class B>
    size_t ObjectPool<D,B>::size(B* p){
        //todo:
        auto iter = find_closed(p);
        if(iter!=ClosedList.end()){
            alloc a = iter->second;
            if(p==a.head){
                return a.head_size;
            }
            //check that p is in range
            if(a.head < p && p < (a.head+(a.head_size * sizeof(D)))){
                size_t offset = p - a.head;
                if(offset % sizeof(D) != 0){
                    throw //mis-alignment
                }
                return a.head_size - (offset/sizeof(D));
            }
        }
        return 0;
    }
    template<class D, class B>
    bool resize(B* &p, size_t N, bool allow_realloc){
        auto iter1 = find_closed(p);
        if(iter!=ClosedList.end() && p==iter->second.head){
            alloc a = iter1->second;
            if(a.head_size>=N){
                //p is shrinking
                iter1->second.head_size = N; //update ClosedList
                a.head += N * sizeof(D);
                a.head_size -= N;
                OpenList.emplace(a.head_size,a);
                return true;
            }
            if(a.head+(N * sizeof(D)) <= a.master+(a.master_size * sizeof(D))){
                //p is a valid sub-allocation, there is enough space after it
                auto range = std::make_pair(OpenList.lower_bound(N),OpenList.upper_bound(a.master_size));
                for(auto iter2 = range.first; iter2 != range.second; ++iter2){
                    alloc b = iter2->second;
                    if(a.master==b.master){
                        //valid allocation block
                        if(b.head==a.head+(a.head_size * sizeof(D))){
                            //this is the sub-allocation that comes after p's sub-allocation
                            if(b.head_size>=N-a.head_size){
                                //sub-allocation has enough available space
                                
                            }
                        }
                    }
                }
            }
            if(Singleton<MemoryMgr>::get().resize(p,N * sizeof(D),allow_realloc)){
            }
        }
        throw 
    }
}