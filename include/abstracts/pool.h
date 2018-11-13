#pragma once

namespace CherylE{
    class PoolAbstract{
    protected:
        using ptr = void*;
        using masterptr = ptr;
        using open_iter = std::map<ptr,alloc>::iterator;
        using openlist_iter = std::multimap<size_t,open_iter>::iterator;
        using closed_iter = std::multimap<ptr,alloc>::iterator;
        using neighbours = std::pair<open_iter,open_iter>;
        struct alloc{
            ptr master;
            ptr head;
            size_t master_size;
            size_t head_size;
        };
        enum fitType{
            bestFit, /*get will return the nearest amount of available memory which fits the query*/
            worstFit /*get will return the largest amount of available memory which fits the query*/
        };
    protected:
        size_t m_free = 0;
        size_t m_used = 0;
        size_t m_total = 0;
        //tracks allocations to prevent memory leaks
        std::unordered_set<masterptr> MasterRecord;
        //lookup table for available allocations
        std::map<ptr,alloc> OpenAllocations;
        //lookup table for available allocations
        std::multimap<size_t,open_iter> OpenList;
        //lookup table for sub-allocations
        std::multimap<ptr,alloc> ClosedList;
    protected:
        virtual closed_iter find_closed(void* p);
        virtual void add_open(const alloc &a);
        virtual void erase_open(openlist_iter &iter);
        virtual void moveto_open(closed_iter iter, size_t N); //needs override if not storing bytes
        virtual openlist_iter find_open(const alloc &a);
        virtual neighbours find_neighbours(void* p);
    public:
        /*frees all memory*/
        virtual ~PoolAbstract(){
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