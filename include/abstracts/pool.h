#pragma once

namespace CherylE{
    // struct ce_ptr{
    //     uintptr_t p = 0;
    //     ce_ptr(){}
    //     ce_ptr(uintptr_t x){
    //         p = x;
    //     }
    //     ce_ptr(void* x){
    //         p = (uintptr_t)x;
    //     }
    //     operator uintptr_t() const {
    //         return p;
    //     }
    //     operator void*() const {
    //         return (void*)p;
    //     }
    //  We need so many operator overloads to replace uintptr_t
    // };
    class AbstractPool{
    protected:
        using ce_uintptr = uintptr_t;
        using openalloc_iter = std::map<ce_uintptr,alloc>::iterator;
        using openlist_iter = std::multimap<size_t,openalloc_iter>::iterator;
        using closed_iter = std::multimap<ce_uintptr,alloc>::iterator;
        using neighbours = std::pair<openalloc_iter,openalloc_iter>;
        struct alloc{
            ce_uintptr master = 0;
            ce_uintptr head = 0;
            size_t master_size = 0;
            size_t head_size = 0;
        };
        enum fitType{
            bestFit, /*get will return the nearest amount of available memory which fits the query*/
            worstFit /*get will return the largest amount of available memory which fits the query*/
        };
        enum resizeResult{
            fail,
            left,
            right/*,
            both/*probably not gonna ever use this*/
        };
    protected:
        size_t type_size = 1;
        size_t m_free = 0;
        size_t m_used = 0;
        size_t m_total = 0;
        //tracks allocations to prevent memory leaks
        std::unordered_set<ce_uintptr> MasterRecord;
        //lookup table for sub-allocations
        std::map<ce_uintptr,alloc> ClosedList;
        //lookup table for available allocations
        std::map<ce_uintptr,alloc> OpenAllocations;
        //lookup table for available allocations
        std::multimap<size_t,openalloc_iter> OpenList;
    protected:
        virtual alloc allocate(const size_t &N) = 0;
        virtual bool isClosed(const ce_uintptr &p);
        virtual bool isOpened(const ce_uintptr &p);
        virtual bool merge(openlist_iter &iter, const alloc &a);
        virtual bool shrink(closed_iter &p_iter, const size_t &N);
        virtual int8_t grow(closed_iter &p_iter, const size_t &N);
        virtual void add_open(const alloc &a);
        virtual void erase_open(openlist_iter &iter);
        virtual void erase_open(openalloc_iter &iter);
        virtual void moveto_open(closed_iter &iter);
        virtual void moveto_open(closed_iter &iter, const size_t &N);
        virtual neighbours find_neighbours(const ce_uintptr &p);
        virtual closed_iter find_closed(const ce_uintptr &p);
        virtual openlist_iter find_open(const alloc &a);
        virtual openalloc_iter find_open(const ce_uintptr &p);
    public:
        /*frees all memory*/
        virtual ~AbstractPool(){
            purge();
        }
        /*frees all memory*/
        virtual void purge();
        /*allocates M blocks of N bytes/objects [order of arguments: N,M]*/
        virtual void pre_allocate(const size_t N, const size_t blocks);
        /**/
        virtual size_t size(void* p);
        /**/
        virtual resizeResult resize(void* &p, const size_t N, bool allow_realloc = false);
        /**/
        virtual void* get(const size_t N, fitType fit = fitType::bestFit);
        /**/
        virtual void put(void* p);
        /**/
        virtual void put(void* p, const size_t N);

        
        /*returns how many bytes/objects are available*/
        size_t free()const{ return m_free; };
        /*returns how many bytes/objects are not available*/
        size_t used()const{ return m_used; };
        /*returns how many bytes/objects are allocated*/
        size_t total()const{ return m_total; };
    };
}