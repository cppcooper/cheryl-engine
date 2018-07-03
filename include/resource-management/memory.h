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
        void* get(size_t count) override {
            Singleton<MemoryMgr>::getInstance().get(count * sizeof(T));
        }
    };
}

#endif