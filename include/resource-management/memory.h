#pragma once
#ifndef MEMORY_H
#define MEMORY_H

#include "../internals.h"
#include "../interfaces/allocator.h"

namespace CherylE
{
    /* MemoryMgr
    ************
    Offers memory management services.
    
    Memory is tracked via three containers.
    1)A master record set which records all allocations.
    2)An open list which records all available sub-allocations
    3)A closed list which records all unavailable sub-allocations

    Services:
    * pre-allocate memory
    * get available memory
    * resize acquired memory
    * return memory
    * purge memory

    Pre-allocating memory is done with chunks. You specify the number of blocks of X width.
    When doing this it is best to consider values that will ideally result in high fragmentation.
    High fragmentation is desirable, as it means all operations can probably be performed.
    */
    class MemoryMgr
    {
        TYPENAMEAVAILABLE_STATIC
    private:
        using ptr = void*;
        using masterptr = ptr;
        struct alloc{
            ptr master;
            ptr head;
            size_t master_size;
            size_t head_size;
        };
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
        alloc allocate(size_t &bytes);
        void open_alloc(closed_iter iter, size_t bytes);
        using closed_iter = std::multimap<masterptr,alloc>::iterator;
        using open_iter = std::multimap<size_t,alloc>::iterator;
        closed_iter find(void* p);

    public:
        /*frees all memory*/
        ~MemoryMgr();
        /*frees all memory*/
        void purge();
        /*allocates N blocks of M bytes [order of arguments: M,N]*/
        void pre_allocate(size_t bytes, size_t blocks = 1);
        /*returns a sub-allocation of X bytes, returns the first fit according to the fitType*/
        void* get(size_t bytes, fitType fit = fitType::bestFit);
        /*finds the sub-allocation p belongs to and returns the number of bytes following after p*/
        size_t size(void* p);
        /*attempts to resize p to X bytes, performs a realloc on p if no other option is available (option disabled by default)*/
        bool resize(void* &p, size_t bytes, bool allow_realloc = false);
        /*returns all the bytes from p to the end of the sub-allocation p belongs to*/
        void put(void* p);
        /*returns all the bytes from p to p+X of the sub-allocation p belongs to*/
        void put(void* p, size_t bytes);
        //void update/cleanup --needs time data

        /*returns how many bytes are available*/
        size_t free()const{ return m_free; };
        /*returns how many bytes are not available*/
        size_t used()const{ return m_used; };
        /*returns how many bytes are allocated*/
        size_t total()const{ return m_total; };
        
        enum fitType{
            bestFit, /*get will return the nearest amount of available memory which fits the query*/
            worstFit /*get will return the largest amount of available memory which fits the query*/
        };
    };

    template<class T>
    class Allocator : public iAllocator
    {
    public:
        void *allocate(size_t count) override {
            return Singleton<MemoryMgr>::getInstance().get(sizeof(T) * count);
        }
        void deallocate(void *ptr, size_t count) override {
            Singleton<MemoryMgr>::getInstance().put(ptr, sizeof(T) * count);
        }
    };
}

#endif