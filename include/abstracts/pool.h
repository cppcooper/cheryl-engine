#pragma once

namespace CherylE{
    class AbstractPool{
    protected:
        using openalloc_iter = std::map<uintptr_t,alloc>::iterator;
        using openlist_iter = std::multimap<size_t,openalloc_iter>::iterator;
        using closed_iter = std::multimap<uintptr_t,alloc>::iterator;
        using neighbours = std::pair<openalloc_iter,openalloc_iter>;
        struct alloc{
            uintptr_t master = 0;
            uintptr_t head = 0;
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
        std::unordered_set<uintptr_t> MasterRecord;
        //lookup table for sub-allocations
        std::map<uintptr_t,alloc> ClosedList;
        //lookup table for available allocations
        std::map<uintptr_t,alloc> OpenAllocations;
        //lookup table for available allocations
        std::multimap<size_t,openalloc_iter> OpenList;
    protected:
        virtual bool isClosed(const uintptr_t p);
        virtual bool isOpened(const uintptr_t p);
        virtual bool merge(openlist_iter iter, const alloc &a);
        virtual bool shrink(closed_iter p_iter, size_t N);
        virtual int8_t grow(closed_iter p_iter, size_t N);
        virtual void add_open(const alloc &a);
        virtual void erase_open(openlist_iter &iter);
        virtual void erase_open(openalloc_iter &iter);
        virtual void moveto_open(closed_iter iter);
        virtual void moveto_open(closed_iter iter, size_t N);
        virtual neighbours find_neighbours(const uintptr_t p);
        virtual closed_iter find_closed(const uintptr_t p);
        virtual openlist_iter find_open(const alloc &a);
        //virtual bool grow(void*)
    public:
        /*frees all memory*/
        virtual ~AbstractPool(){
            purge();
        }
        /*frees all memory*/
        virtual void purge() = 0;
        /*allocates M blocks of N bytes/objects [order of arguments: N,M]*/
        virtual void pre_allocate(size_t N, size_t blocks) = 0;
        /*returns how many bytes/objects are available*/
        size_t free()const{ return m_free; };
        /*returns how many bytes/objects are not available*/
        size_t used()const{ return m_used; };
        /*returns how many bytes/objects are allocated*/
        size_t total()const{ return m_total; };
    };
}