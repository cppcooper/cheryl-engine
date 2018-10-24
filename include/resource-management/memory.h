#pragma once
#ifndef MEMORY_H
#define MEMORY_H

#include "../internals.h"
#include "../interfaces/allocator.h"

namespace CherylE
{
    class MemoryMgr
    {
    private:
        uint32_t default_alloc_size;
        //std::set<pointer> MasterRecord
        //std::unordered_multimap<available_length, pointer> OpenList
        //unordered does not have `lower_bound` method

        /*  data:
                allocation
                    -head
                    -length

                available allocation
                    -pointer to master allocation
                    -head
                    -length
        */
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