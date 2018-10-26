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
        using masterptr = ptr;
        using ptr = uint64_t;
        struct alloc{
            ptr master;
            ptr head;
            size_t master_size;
            size_t head_size;
        };
        size_t default_alloc_size;
        size_t m_free;
        size_t m_used;
        size_t m_total;
        //tracks allocations to prevent memory leaks
        std::unordered_set<masterptr> MasterRecord;
        //lookup table for available allocations
        std::multimap<size_t,alloc> OpenList;
        //lookup table for sub-allocations
        std::multimap<masterptr,alloc> ClosedList;

    public:
        void set_default_alloc_size(size_t alloc_size){default_alloc_size=alloc_size;}
        //void* reserve(size_t bytes);
        void* get(size_t bytes);
        void* get(size_t bytes, size_t alloc_size);
        size_t size(void* ptr);
        bool resize(void* ptr, size_t bytes);
        void put(void* ptr);
        void put(void* ptr, size_t bytes);
        //void update/cleanup --needs time data
        size_t free()const{ return m_free; };
        size_t used()const{ return m_used; };
        size_t total()const{ return m_total; };
    };

    template<class T>
    class Allocator : public iAllocator
    {
    public:
        void *allocate(size_t count) override {
            //size_t alloc_size = Factory<T>::alloc_size??
            return Singleton<MemoryMgr>::getInstance().get(sizeof(T) * count);
        }
        void deallocate(void *ptr, size_t count) override {
            Singleton<MemoryMgr>::getInstance().put(ptr, sizeof(T) * count);
        }
    };
}

#endif