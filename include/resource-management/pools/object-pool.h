#pragma once

namespace CherylE{
    template<class derived, class base>
    class ObjectPool : public iPoolT<base>{
        TYPENAMEAVAILABLE_STATIC
        static_assert(isderived(base,derived),"In ObjectPool<D,B>, D must be derived from B.");
    protected:
        virtual alloc allocate(const size_t &N) override;
    public:
        ObjectPool(){
            type_size = sizeof(derived);
        }
        /*frees all memory*/
        virtual void purge() override;
        /*returns an array of size N*/
        virtual base* getT(const size_t N, const fitType fit = fitType::worstFit) override;
    };

    template<class D, class B>
    alloc ObjectPool<D,B>::allocate(const size_t N){
        void* p = Singleton<MemoryMgr>::get().get(N * type_size); //exception when N=0
        ce_uintptr ptr = (ce_uintptr)p;
        MasterRecord.emplace(ptr);
        m_total += bytes;
        m_free += bytes;
        return alloc{ptr,ptr,N,N};;
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
    B* ObjectPool<D,B>::getT(const size_t N, const fitType fit){
        D* p = get(N,fit);
        for(int i = 0; i < N; ++i){
            p[i] = D();
        }
        return p;
    }
}