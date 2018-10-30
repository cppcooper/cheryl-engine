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
        using ptr = void*;
        using masterptr = ptr;
        struct alloc{
            ptr master;
            ptr head;
            size_t master_size;
            size_t head_size;
        };
        size_t m_free;
        size_t m_used;
        size_t m_total;
        //tracks allocations to prevent memory leaks
        std::unordered_set<masterptr> MasterRecord;
        //lookup table for available allocations
        std::multimap<size_t,alloc> OpenList;
        //lookup table for sub-allocations
        std::multimap<masterptr,alloc> ClosedList;

    protected:
        alloc allocate(size_t &bytes);
        using closed_iter = std::multimap<masterptr,alloc>::iterator;
        void open_alloc(closed_iter iter, size_t bytes);
        closed_iter find(void* p);

    public:
        void pre_allocate(size_t bytes, size_t blocks = 1);
        void* get(size_t bytes, fitType fit = fitType::bestFit);
        size_t size(void* p);
        bool resize(void* p, size_t bytes, bool allow_realloc = false);
        void put(void* p);
        void put(void* p, size_t bytes);
        //void update/cleanup --needs time data
        size_t free()const{ return m_free; };
        size_t used()const{ return m_used; };
        size_t total()const{ return m_total; };
        enum fitType{
            bestFit,
            worstFit
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