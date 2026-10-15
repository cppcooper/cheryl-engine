#pragma once
#ifndef ABSTRACT_POOL_H
#define ABSTRACT_POOL_H

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

// struct ce_ptr{
//     uintptr_t p = 0;
//     ce_ptr(){}
//     ce_ptr(uintptr_t x){
//         p = x;
//     }
//     ce_ptr(void* x){
//         p = (uintptr_t)x;
//     }
//     operator uintptr_t() const {
//         return p;
//     }
//     operator void*() const {
//         return (void*)p;
//     }
//  We need so many operator overloads to replace uintptr_t
// };
namespace CherylE {
    enum fitType {
        lower_bound, // first allocation big enough for request
        upper_bound, // first allocation greater than request
        second_largest, // todo: drop? rename?
        largest // largest allocation available
    };
    enum resizeResult {
        fail,
        realloc,
        shrunk,
        left,
        right/*,
        both/*probably not gonna ever use this*/
    };

    namespace Memory {
        struct allocation_block {
            const uintptr_t head;
            const size_t length;
        };

        struct allocation {
            allocation_block owning_block;
            allocation_block parent_block;
            allocation_block block;
        };
    }


    using ablock = Memory::allocation_block;
    using alloc = Memory::allocation;
    using OpenAllocMap = std::map<uintptr_t, alloc>;
    using oam_iter = OpenAllocMap::iterator;
    using openlist_iter = std::multimap<size_t, oam_iter>::iterator;
    using closed_iter = std::multimap<uintptr_t, alloc>::iterator;
    using neighbours = std::pair<oam_iter, oam_iter>;

    // todo: if this is going to be the singleton, then there can't be pure virtuals
    // todo: remove T, no longer makes sense if alloc is in bytes as MemoryPool then is generalized for bytes
    template<typename T, size_t growth_factor = 2, size_t base_growth = 4>
    class MemoryPool {
    TYPENAMEAVAILABLE_VIRTUAL;
        static_assert(base_growth > 0, "The base growth should be a positive integer.");
        static_assert(growth_factor > 0, "The growth factor cannot be 0.");
        static_assert(growth_factor < 32, "Use a reasonable growth factor that is below 32.");
    protected:
        size_t m_free = 0;
        size_t m_used = 0;
        size_t m_total = 0;

        //tracks allocations to prevent memory leaks
        std::unordered_set<uintptr_t> MasterRecord;
        //lookup table for sub-allocations
        std::multimap<uintptr_t, alloc> ClosedList;
        //lookup table for available allocations
        std::map<uintptr_t, alloc> OpenAllocations;
        //lookup table for available allocations
        std::multimap<size_t, oam_iter> OpenList;

    protected:

        bool isOnClosed(const uintptr_t &p);

        bool isOnOpened(const uintptr_t &p);

        bool merge(openlist_iter iter, const alloc &a);

        bool shrink(closed_iter p_iter, const size_t &N);

        int8_t growBy(closed_iter p_iter, const size_t &N);

        int8_t growTo(closed_iter p_iter, const size_t &N);

        void add_open(const alloc &a);

        void erase_open(openlist_iter iter);

        void erase_open(oam_iter iter);

        void moveto_open(closed_iter iter, const uintptr_t &p, const size_t &N);

        void moveto_open(closed_iter iter);

        neighbours find_neighbours(const uintptr_t &p);

        closed_iter find_closed(const uintptr_t &p);

        openlist_iter find_open(const alloc &a);

        oam_iter find_open(const uintptr_t &p);
    protected:

        MemoryPool() = default;

        virtual alloc allocate_impl(const size_t &N) = 0;

        template<fitType fit>
        alloc allocate(const size_t &N);
    public:

        /*frees all memory*/
        virtual void purge() = 0;

        /*frees all memory*/
        virtual ~MemoryPool() {
            purge();
        }

        /*allocates M blocks of N bytes/objects [order of arguments: N,M]*/
        void pre_allocate(const size_t &N, const size_t &blocks);

        /**/
        size_t size(void* p);

        /**/
        resizeResult resize(void* &p, const size_t &N, bool allow_realloc = false);

        /**/
        template<fitType fit = fitType::lower_bound>
        void* get(const size_t N);

        /**/
        void put(const void* p);

        /**/
        void put(const void* p, const size_t &N);

        /*returns how many bytes/objects are available*/
        size_t free() const { return m_free; };

        /*returns how many bytes/objects are not available*/
        size_t used() const { return m_used; };
        /*returns how many bytes/objects are allocated*/
        size_t total() const { return m_total; };
    };


}

#endif
