#pragma once
#ifndef CEPOOL_H
#define CEPOOL_H

namespace CherylE{
    class iPool{
    protected:
        using ptr = void*;
        using masterptr = ptr;
        size_t m_free = 0;
        size_t m_used = 0;
        size_t m_total = 0;
        //tracks allocations to prevent memory leaks
        std::unordered_set<masterptr> MasterRecord;
        //lookup table for available allocations
        std::multimap<size_t,alloc> OpenList;
        //lookup table for sub-allocations
        std::multimap<masterptr,alloc> ClosedList;
    protected:
        using closed_iter = std::multimap<masterptr,alloc>::iterator;
        using open_iter = std::multimap<size_t,alloc>::iterator;
        virtual void moveto_open(closed_iter iter, size_t N) = 0;
        virtual closed_iter find_closed(void* p) = 0;
    public:
        /*frees all memory*/
        virtual ~iPool(){
            purge();
        }
        /*frees all memory*/
        virtual void purge() = 0;
        /*allocates M blocks of N bytes/objects [order of arguments: N,M]*/
        virtual void pre_allocate(size_t N, size_t blocks) = 0;
        /*returns an array of size N*/
        virtual void* get(size_t N) = 0;
        /*returns size of array*/
        virtual size_t size(void* p) = 0;
        /*attempts to resize p to N elements, if allowed will reallocate if no other option is available*/
        virtual bool resize(void* &p, size_t N, bool allow_realloc/* = false*/) = 0;
        /*returns all the elements from p to the end of the array p belongs to*/
        virtual void put(void* p) = 0;
        /*returns all the elements from p to p+N of the array p belongs to*/
        virtual void put(void* p, size_t N) = 0;
        
        /*returns how many bytes/objects are available*/
        size_t free()const{ return m_free; };
        /*returns how many bytes/objects are not available*/
        size_t used()const{ return m_used; };
        /*returns how many bytes/objects are allocated*/
        size_t total()const{ return m_total; };
    };
}

#endif