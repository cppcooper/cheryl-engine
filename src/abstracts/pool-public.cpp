#include <abstracts/pool.h>
#include <internals.h>

#include <cinttypes>

#include <iterator>
#include <vector>
#include <cmath>
using std::size_t;

namespace CherylE {
    template<size_t growth_factor, size_t base_growth>
    void MemoryPool<growth_factor, base_growth>::pre_allocate(const size_t N, const size_t blocks) {
        if (blocks < 1) {
            throw invalid_args(__CEFUNCTION__, __LINE__, "The number of allocated blocks cannot be less than 1.");
        }
        for (int i = 0; i < blocks; ++i) {
            add_open(allocate(N));
        }
    }

    template<size_t growth_factor, size_t base_growth>
    size_t MemoryPool<growth_factor, base_growth>::size(void* p) {
        auto ptr = (uintptr_t) p;
        if (isOnClosed(ptr)) {
            return find_closed(ptr)->second.head_size;
        }
        auto iter = find_open(ptr);
        if (iter != OpenAllocations.end()) {
            return iter->second.head_size;
        }
        return 0;
    }

    template<size_t growth_factor, size_t base_growth>
    resizeResult MemoryPool<growth_factor, base_growth>::resize(void* &p, const size_t N, bool allow_realloc) {
        auto ip = (uintptr_t) p;
        auto iter = find_closed(ip);
        if (iter == ClosedList.end()) {
            throw invalid_args(__CEFUNCTION__, __LINE__, "Invalid pointer, it is not managed on the ClosedList.");
        }
        alloc &a = iter->second;
        if (ip != a.head) {
            throw invalid_args(__CEFUNCTION__, __LINE__, "Invalid pointer. Must use the head of the sub-allocation.");
        }
        if (N == a.head_size) {
            throw bad_request(__CEFUNCTION__, __LINE__, "Resize must change the head size. This request was where `new_size = head_size`");
        }

        //Done with error checking
        //doing the resizing now
        if (N < a.head_size) {
            if (!shrink(iter, N)) {
                throw failed_operation(__CEFUNCTION__, __LINE__, "Shrink operation failled.");
            }
            return resizeResult::shrunk;
        } else {
            int8_t r = grow(iter, N);
            if (r == 0) {
                if (allow_realloc) {
                    void* p2 = get(N);
                    std::memcpy(p2, p, a.head_size);
                    put(p);
                    p = p2;
                    return resizeResult::realloc;
                }
            } else {
                return r < 0 ? resizeResult::left : resizeResult::right;
            }
        }
        return resizeResult::fail;
    }

    template<size_t growth_factor, size_t base_growth>
    template<fitType fit>
    void* MemoryPool<growth_factor, base_growth>::get(const size_t N) {
        auto iter = OpenList.end();
        auto checkFit = [&N](alloc &a) {
            return a.head_size >= N;
        };
        if (OpenList.empty()) {
            //pre_allocate(2 * N, 2);
            return (void*) getNew(N).head;
        }

        /*
         * We decide at compile time how to fulfill the get request, and so why not constexpr our logic
         * OpenList is ordered from the smallest to the largest keys
         */
        if constexpr (fit == fitType::lower_bound) {
            // lower bound gets us the smallest key that is NOT less than N
            iter = OpenList.lower_bound(N);
        } else if constexpr (fit == fitType::upper_bound) {
            // upper bound gets us the first key greater than N
            iter = OpenList.upper_bound(N);
            if (iter == OpenList.end()) {
                iter = OpenList.lower_bound(N);
            }
        } else if constexpr (fit == fitType::largest) {
            // rbegin() points to the largest allocation (but rbegin.base() == end())
            iter = (++OpenList.rbegin()).base();
        } else if constexpr (fit == fitType::second_largest) {
            auto riter = OpenList.rbegin();
            std::advance(riter, std::min(2ul, OpenList.size()));
            iter = riter.base();
        }

        // did we find the fit we want?
        if (iter != OpenList.end() || !checkFit(iter->second->second)) {
            size_t N2;
            if constexpr (fit == fitType::lower_bound) {
                N2 = N;
            } else if constexpr (fit == fitType::upper_bound) {
                N2 = N + base_growth;
            } else if constexpr (fit == fitType::largest) {
                N2 = N * growth_factor + base_growth;
            } else if constexpr (fit == fitType::second_largest) {
                N2 = N * (growth_factor - 1) + base_growth;
            }
            // todo: revise getNew?
            //  don't emplace? & early return?
            iter = OpenList.emplace(N2, getNew(N2));
        }

        alloc &a = iter->second->second;
        erase_open(iter);
        ClosedList.emplace(a.head, a);
        return (void*) a.head;
    }

    template<size_t growth_factor, size_t base_growth>
    void MemoryPool<growth_factor, base_growth>::put(const void* p) {
        moveto_open(find_closed((uintptr_t) p));
    }

    template<size_t growth_factor, size_t base_growth>
    void MemoryPool<growth_factor, base_growth>::put(const void* p, const size_t N) {
        auto ip = (uintptr_t)p;
        moveto_open(find_closed(ip), ip, N);
    }
}
