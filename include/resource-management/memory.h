#pragma once
#ifndef MEMORY_H
#define MEMORY_H

#include "../internals.h"
#include "../interfaces/pool.h"
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
    class MemoryMgr : public iPool{
        TYPENAMEAVAILABLE_STATIC
    protected:
        alloc allocate(size_t &bytes);
        void moveto_open(closed_iter iter, size_t bytes);
        closed_iter find_closed(void* p);
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