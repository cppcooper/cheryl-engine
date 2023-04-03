#include <abstracts/pool.h>
#include <internals.h>

#include <cinttypes>

using std::size_t;

namespace CherylE {

    void MemoryPool::pre_allocate(const size_t N, const size_t blocks) {
        if (blocks < 1) {
            throw invalid_args(__CEFUNCTION__, __LINE__, "The number of allocated blocks cannot be less than 1.");
        }
        for (int i = 0; i < blocks; ++i) {
            add_open(allocate(N));
        }
    }

    size_t MemoryPool::size(void* p) {
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

    resizeResult MemoryPool::resize(void* &p, const size_t N, bool allow_realloc) {
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

    void* MemoryPool::get(const size_t N, fitType fit) {
        CherylE::openlist_iter iter;
        if (fit == fitType::bestFit) {
            iter = OpenList.lower_bound(N);
        } else if (fit == fitType::worstFit) {
            iter = OpenList.end();
            if (iter != OpenList.begin()) {
                --iter;
            }
        }
        if (iter != OpenList.end()) {
            alloc &a = iter->second->second;
            erase_open(iter);
            ClosedList.emplace(a.head, a);
            return (void*) a.head;
        } else {
            alloc a = allocate(N);
            if (a.head_size == N) {
                ClosedList.emplace(a.head, a);
            } else {
                uintptr_t temp = a.head;
                size_t remainder = a.head_size - N;
                a.head_size = N;
                ClosedList.emplace(a.head, a);
                a.head += a.head_size;
                a.head_size = remainder;
                add_open(a);
                a.head = temp;
            }
            return (void*) a.head;
        }
    }

    void MemoryPool::put(const void* p) {
        moveto_open(find_closed((uintptr_t) p));
    }

    void MemoryPool::put(const void* p, const size_t N) {
        moveto_open(find_closed((uintptr_t) p), N);
    }
}
