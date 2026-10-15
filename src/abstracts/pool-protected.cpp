#include <abstracts/pool.h>
#include <internals.h>
#include <cinttypes>
#include <cassert>

using std::size_t;

namespace CherylE {

    inline bool inRange(const size_t ts, const uintptr_t p, const uintptr_t r, const size_t s) {
        return (r <= p && p < r + (ts * s));
    }

    inline bool inRange(const size_t &ts, const uintptr_t &p, const size_t &ps, const uintptr_t &r, const size_t &rs) {
        return (r <= p && p + (ts * ps) <= r + (ts * rs));
    }

    inline bool in_range(const alloc &a, const uintptr_t &p) {
        return a.block.head <= p && p <= a.block.head + a.block.length;
    }

    inline bool range_in_range(const uintptr_t &p, const size_t &N, const alloc &outer_range) {
        return outer_range.block.head <= p && p + N <= outer_range.block.head + outer_range.block.length;
    }


    template<typename alloc_left, typename alloc_right>
    inline bool is_contiguous(alloc_left&& left, alloc_right&& right) {
        using al = std::remove_cvref_t<alloc_left>;
        using ar = std::remove_cvref_t<alloc_right>;
        static_assert(std::is_same_v<ablock, al> || std::is_same_v<alloc, al>);
        static_assert(std::is_same_v<al, ar>);
        if constexpr (std::is_same_v<ablock, al>) {
            return left.head + left.length == right.head;
        } else {
            return left.owning_block.head == right.owning_block.head &&
                   left.block.head + left.block.length == right.block.head;
        }
    }

    template<typename T>
    bool is_aligned(const void * ptr) noexcept {
        auto iptr = reinterpret_cast<std::uintptr_t>(ptr);
        return !(iptr % alignof(T));
    }

    template<typename C, typename I, typename A> // the letters are a coincidence
    inline bool update_openList(C &OpenList, I iter, const A &a) {
        if (iter != OpenList.end()) {
            auto second = iter->second;
            OpenList.erase(iter);
            OpenList.emplace(a.head_size, second);
            return true;
        }
        return false;
    }

    template<typename C, typename I, typename A>
    inline bool update_closedList(C &ClosedList, I iter, const A &a) {
        if (iter != ClosedList.end()) {
            ClosedList.erase(iter);
            ClosedList.emplace(a.head, a);
            return true;
        }
        return false;
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    bool MemoryPool<T, growth_factor, base_growth>::isOnClosed(const uintptr_t &p) {
        return find_closed(p) != ClosedList.end();
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    bool MemoryPool<T, growth_factor, base_growth>::isOnOpened(const uintptr_t &p) {
        return find_open(p) != OpenAllocations.end();
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    bool MemoryPool<T, growth_factor, base_growth>::merge(openlist_iter iter, const alloc &a) {
        if (iter != OpenList.end()) {
            alloc &b = iter->second->second;
            if (is_contiguous(b, a)) {
                //merging in on the right
                b.head_size += a.head_size;
                return update_openList(OpenList, iter, b);
            }
            if (is_contiguous(a, b)) {
                //merging in on the left
                b.head = a.head;
                b.head_size += a.head_size;
                return update_openList(OpenList, iter, b);
            }
            return false;
        }
        throw invalid_args(_CE_HERE, "Invalid iterator.");
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    int8_t MemoryPool<T, growth_factor, base_growth>::growBy(closed_iter p_iter, const size_t &N) {
        if (p_iter->second.head != p_iter->second.master) {
            // todo: make new allocation, copy this to it, return p_iter
        }
        if (p_iter != ClosedList.end()) {
            alloc &a = p_iter->second;
            assert(N > a.head_size && "Allocation is not less than the growBy parameter N");
            auto neighbours = find_neighbours(p_iter->second.head);
            size_t needed = N - a.head_size;
            alloc left, right;
            if (neighbours.second != OpenAllocations.end()) {
                right = neighbours.second->second;
            }
            if (neighbours.first != OpenAllocations.end()) {
                left = neighbours.first->second;
            }

            //Check available space (right, then left)
            if (right.head_size >= needed) {
                a.head_size = N;
                update_closedList(ClosedList, p_iter, a);

                alloc &b = neighbours.second->second;
                b.head_size -= needed;
                if (b.head_size == 0) {
                    erase_open(neighbours.second);
                } else {
                    b.head = a.head + a.head_size;
                    if (!update_OpenList(OpenList, find_open(b), b)) {
                        throw failed_operation(_CE_HERE, "Failed to find OpenList iterator.");
                    }
                }
                return 1;
            }
            if (left.head_size >= needed) {
                a.head -= needed;
                a.head_size = N;
                update_closedList(ClosedList, p_iter, a);

                alloc &b = neighbours.first->second;

                b.head_size -= needed;
                if (b.head_size == 0) {
                    erase_open(neighbours.second);
                } else {
                    // todo: b_openlist_iter, this whole block looks wrong
                    b_ol_iter->first = b.head_size;
                    if (!update(OpenList, find_open(b), b)) {
                        throw failed_operation(_CE_HERE, "Failed to find OpenList iterator.");
                    }
                }
                return -1;
            }
        }
        return 0;
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    int8_t MemoryPool<T, growth_factor, base_growth>::growTo(closed_iter p_iter, const size_t &N) {
        assert(N > p_iter->second.head_size);
        return growBy(p_iter, N - p_iter->second.head_size);
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    bool MemoryPool<T, growth_factor, base_growth>::shrink(closed_iter p_iter, const size_t &N) {
        if (p_iter != ClosedList.end()) {
            alloc &a = p_iter->second;
            assert(N < a.head_size && "Allocation is not greater than the shrink parameter N");

            size_t remainder = a.head_size - N;
            a.head_size = N;

            if (remainder != 0) {
                alloc b{a.master, a.head + a.head_size, a.master_size, remainder};
                auto right_al_iter = find_neighbours(b.head).second;
                if (right_al_iter != OpenAllocations.end()) {
                    alloc &right = right_al_iter->second;
                    if (is_contiguous(b, right)) {
                        right.head = b.head;
                        right.head_size += b.head_size;
                        if (!update(OpenList, find_open(right), right)) {
                            throw failed_operation(_CE_HERE, "Failed to find OpenList iterator.");
                        }
                    } else {
                        add_open(right);
                    }
                }
            }
            return true;
        }
        return false;
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    void MemoryPool<T, growth_factor, base_growth>::add_open(const alloc &a) {
        auto result = OpenAllocations.emplace(a.head, a);
        if (!result.second) {
            throw bad_request(_CE_HERE, "Allocation already exists in OpenAllocations");
        }
        // todo: check for pre-existing allocation in another way
        if (find_open(a) != OpenList.end()) {
            throw bad_request(_CE_HERE, "Allocation already exists in OpenList");
        }
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    void MemoryPool<T, growth_factor, base_growth>::erase_open(openlist_iter iter) {
        if (iter == OpenList.end()) {
            throw invalid_args(_CE_HERE);
        }
        OpenAllocations.erase(iter->second);
        OpenList.erase(iter);
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    void MemoryPool<T, growth_factor, base_growth>::erase_open(oam_iter iter) {
        if (iter == OpenAllocations.end()) {
            throw invalid_args(_CE_HERE);
        }
        alloc a = iter->second;
        auto iter2 = find_open(a);
        if (iter2 == OpenList.end()) {
            throw bad_request(_CE_HERE, "Cannot find the OpenList iterator");
        }
        OpenAllocations.erase(iter);
        OpenList.erase(iter2);
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    void MemoryPool<T, growth_factor, base_growth>::moveto_open(closed_iter iter, const uintptr_t &p, const size_t &N) {
        if (iter == ClosedList.end()) {
            throw invalid_args(_CE_HERE, "Invalid iterator, p is probably already on the OpenList.");
        }

        alloc &ca = iter->second; // this is the original closed allocation
        if (!range_in_range(p, N, ca)) {
            throw bad_request(_CE_HERE, "Range [p,p+N] goes beyond the bounds of the allocation block in iter.");
        }

        /* 1. get open neighbouring blocks to p
         * 2. check if they actually touch p's block
         * 3. if they do, merge them into p's block, and remove the merged blocks from the OpenList
         * 4. then add p's block to the open list
         * 5. remove iter from the ClosedList
         * 6. if iter's block still exists add it back to the ClosedList with the new size
         */
        auto neighbours = find_neighbours(ca.block.head);
        alloc empty{0};
        alloc &n_left = neighbours.first != OpenAllocations.end() ? neighbours.first->second : empty; // neighbour left of p
        alloc &n_right = neighbours.second != OpenAllocations.end() ? neighbours.second->second : empty; // neighbour right of p
        ablock block_to_open = {p, N};
        uintptr_t bto_head = block_to_open.head;
        size_t bto_length = block_to_open.length;

        // Now we're going to merge blocks if we can
        if (n_left.owning_block.head != 0 && is_contiguous(n_left.block, block_to_open)) {
            auto ol_left_iter = find_open(n_left);
            auto la = ol_left_iter->second->second;
            OpenAllocations.erase(ol_left_iter->second);
            OpenList.erase(ol_left_iter);
            bto_head = la.block.head;
            bto_length += la.block.length;
        }
        if (n_right.owning_block.head != 0 && is_contiguous(block_to_open, n_right.block)) {
            auto ol_right_iter = find_open(n_right);
            auto ra = ol_right_iter->second->second;
            OpenAllocations.erase(ol_right_iter->second);
            OpenList.erase(ol_right_iter);
            bto_length += ra.block.length;
        }
        alloc oa = {
                ca.owning_block,
                ca.parent_block,
                {bto_head, bto_length}
        };
        // move oa to the OpenList, and remove iter from the ClosedList (before we add ca back with its new length)
        OpenList.emplace(oa.block.length, OpenAllocations.emplace(oa.block.head, oa));
        ClosedList.erase(iter);
        size_t ca_length = ca.block.length - N;
        if (ca_length > 0) {
            ClosedList.emplace(ca.block.head, alloc{
                ca.owning_block,
                ca.parent_block,
                {ca.block.head, ca_length}
            });
        }
        m_free += N;
        m_used -= N;
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    void MemoryPool<T, growth_factor, base_growth>::moveto_open(closed_iter iter) {
        if (iter == ClosedList.end()) {
            throw invalid_args(_CE_HERE, "Invalid iterator, iter == ClosedList.end().");
        }
        moveto_open(iter, iter->second.block.head, iter->second.block.length);
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    neighbours MemoryPool<T, growth_factor, base_growth>::find_neighbours(const uintptr_t &p) {
        auto iter = find_closed(p);
        auto end = OpenAllocations.end();
        // if p isn't on the closed list, it is either invalid or on the OpenList
        if (iter == ClosedList.end()) {
            // todo: throw an exception?
            return std::make_pair(end, end);
        }
        alloc &p_alloc = iter->second;

        oam_iter left, right;
        // get the iterator to the first open allocation that doesn't come before p
        left = right = OpenAllocations.lower_bound(p);
        left != OpenAllocations.begin() ?
        --left : left = end;

        if (left != end && !is_contiguous(left->second, p_alloc)) {
            left = end;
        }
        if (right != end && !is_contiguous(p_alloc, right->second)) {
            right = end;
        }
        return std::make_pair(left, right);
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    closed_iter MemoryPool<T, growth_factor, base_growth>::find_closed(const uintptr_t &p) {
        auto iter = ClosedList.lower_bound(p);
        if (iter != ClosedList.end()) {
            if (iter->second.head != p) {
                if (iter != ClosedList.begin()) {
                    --iter;
                    // todo: figure out inRange math, and what we're missing for the call (look for other usages, that don't have errors)
                    if (!inRange(type_size, p, iter->second.head, iter->second.head_size)) {
                        iter = ClosedList.end();
                    }
                } else {
                    iter = ClosedList.end();
                }
            }
        }
        return iter;
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    openlist_iter MemoryPool<T, growth_factor, base_growth>::find_open(const alloc &a) {
        auto iter = OpenList.lower_bound(a.head_size);
        for (; iter != OpenList.end(); ++iter) {
            alloc &v = iter->second->second;
            if (v.head_size != a.head_size) {
                return OpenList.end();
            } else if (v.head == a.head) {
                return iter;
            }
        }
        return OpenList.end();
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    oam_iter MemoryPool<T, growth_factor, base_growth>::find_open(const uintptr_t &p) {
        auto iter = OpenAllocations.lower_bound(p);
        if (iter != OpenAllocations.end()) {
            if (iter->second.head != p) {
                if (iter != OpenAllocations.begin()) {
                    --iter;
                    if (!inRange(type_size, p, iter->second.head, iter->second.head_size)) {
                        iter = OpenAllocations.end();
                    }
                } else {
                    iter = OpenAllocations.end();
                }
            }
        }
        return iter;
    }

    template<typename T, size_t growth_factor, size_t base_growth>
    template<fitType fit>
    alloc MemoryPool<T, growth_factor, base_growth>::allocate(const size_t &N) {
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
        return allocate_impl(N2);
    }
}
