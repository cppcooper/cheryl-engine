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
        virtual alloc allocate(const size_t &bytes) override;
    public:
        virtual void purge() override;
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