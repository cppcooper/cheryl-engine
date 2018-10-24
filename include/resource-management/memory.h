#pragma once
#ifndef MEMORY_H
#define MEMORY_H

#include <unordered_set>
#include "../internals.h"
#include "../interfaces/allocator.h"

namespace CherylE
{
    class MemoryMgr
    {
    private:
        using ptr = uint64_t;
        using masterptr = ptr;
        using alloc = std::pair<ptr,uint32_t>;
        uint32_t default_alloc_size;
        std::unordered_set<masterptr> MasterRecord; //tracks allocations to prevent memory leaks
        std::multimap<uint32_t,std::pair<masterptr,ptr>> OpenList; //lookup table for available allocations
        std::unordered_multimap<masterptr,alloc> ClosedList; //lookup table for sub-allocations
        
    public:
        static const char* TypeName() const {
            static const char* name = "MemoryMgr";
            return name;
        }
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