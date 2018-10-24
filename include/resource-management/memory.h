#pragma once
#ifndef MEMORY_H
#define MEMORY_H

#include "../internals.h"
#include "../interfaces/allocator.h"

namespace CherylE
{
    class MemoryMgr
    {
        TYPENAMEAVAILABLE_STATIC
    private:
        using ptr = uint64_t;
        using masterptr = ptr;
        using alloc = std::pair<ptr,uint32_t>;
        uint32_t default_alloc_size;
        //tracks allocations to prevent memory leaks
        std::unordered_set<masterptr> MasterRecord;
        //lookup table for available allocations
        std::multimap<uint32_t,std::pair<masterptr,ptr>> OpenList;
        //lookup table for sub-allocations
        std::unordered_multimap<masterptr,alloc> ClosedList;

    public:
        void* get(size_t bytes);
        void put(void* ptr, size_t bytes);
        uint64_t length();
        uint64_t size();
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