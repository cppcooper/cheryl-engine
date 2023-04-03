#include <abstracts/pool.h>
#include <internals.h>
#include <cinttypes>
#include <cassert>

using std::size_t;

namespace CherylE {


    using openalloc_iter = std::map<uintptr_t, alloc>::iterator;
    using openlist_iter = std::multimap<size_t, openalloc_iter>::iterator;
    using closed_iter = std::multimap<uintptr_t, alloc>::iterator;
    using neighbours = std::pair<openalloc_iter, openalloc_iter>;
    inline bool inRange(const size_t ts, const uintptr_t p, const uintptr_t r, const size_t s) {
        return (r <= p && p < r + (ts * s));
    }

    inline bool inRange(const size_t &ts, const uintptr_t &p, const size_t &ps, const uintptr_t &r, const size_t &rs) {
        return (r <= p && p + (ts * ps) <= r + (ts * rs));
    }

    inline bool isAligned(const size_t &ts, const uintptr_t &a, const uintptr_t &b, const size_t &bs) {
        return a == b + (ts * bs);
    }

    inline bool update(std::multimap<size_t, openalloc_iter> &OpenList, openlist_iter iter, const alloc &a) {
        if (iter != OpenList.end()) {
            auto second = iter->second;
            OpenList.erase(iter);
            OpenList.emplace(a.head_size, second);
            return true;
        }
        return false;
    }

    inline bool update(std::multimap<uintptr_t, alloc> &ClosedList, closed_iter iter, const alloc &a) {
        if (iter != ClosedList.end()) {
            ClosedList.erase(iter);
            ClosedList.emplace(a.head, a);
            return true;
        }
        return false;
    }

    bool MemoryPool::isOnClosed(const uintptr_t &p) {
        return find_closed(p) != ClosedList.end();
    }

    bool MemoryPool::isOnOpened(const uintptr_t &p) {
        return find_open(p) != OpenAllocations.end();
    }

    bool MemoryPool::merge(openlist_iter iter, const alloc &a) {
        if (iter != OpenList.end()) {
            alloc &b = iter->second->second;
            if (isAligned( type_size, a.head, b.head, b.head_size)) {
                //merging in on the right
                b.head_size += a.head_size;
                return update(OpenList, iter, b);
            }
            if (isAligned(type_size, b.head, a.head, a.head_size)) {
                //merging in on the left
                b.head = a.head;
                b.head_size += a.head_size;
                return update(OpenList, iter, b);
            }
            return false;
        }
        throw invalid_args(_CE_HERE, "Invalid iterator.");
    }

    int8_t MemoryPool::grow(closed_iter p_iter, const size_t &N) {
        if (p_iter != ClosedList.end()) {
            alloc &a = p_iter->second;
            assert(N > a.head_size && "Allocation is not less than the grow parameter N");
            auto neighbours = find_neighbours(p_iter->second.head);
            // todo: is bytes the object size? ie. type_size
            size_t bytes_needed = bytes - a.head_size;
            alloc left, right;
            if (neighbours.second != OpenAllocations.end()) {
                right = neighbours.second->second;
            }
            if (neighbours.first != OpenAllocations.end()) {
                left = neighbours.first->second;
            }

            //Check available space (right, then left)
            if (right.head_size >= bytes_needed) {
                a.head_size = N;
                update(ClosedList, p_iter, a);

                alloc &b = neighbours.second->second;
                b.head_size -= bytes_needed;
                if (b.head_size == 0) {
                    // todo: figure out why erase_open is no good for our usages
                    erase_open(neighbours.second->second);
                } else {
                    b.head = a.head + a.head_size;
                    if (!update(OpenList, find_open(b), b)) {
                        throw failed_operation(__CEFUNCTION__, __LINE__, "Failed to find OpenList iterator.");
                    }
                }
                return 1;
            }
            if (left.head_size >= bytes_needed) {
                a.head -= bytes_needed;
                a.head_size = N;
                update(ClosedList, p_iter, a);

                alloc &b = neighbours.first->second;
                b.head_size -= bytes_needed;
                if (b.head_size == 0) {
                    erase_open(neighbours.second->second);
                } else {
                    // todo: b_openlist_iter, this whole block looks wrong
                    b_ol_iter->first = b.head_size;
                    if (!update(OpenList, find_open(b), b)) {
                        throw failed_operation(__CEFUNCTION__, __LINE__, "Failed to find OpenList iterator.");
                    }
                }
                return -1;
            }
        }
        return 0;
    }

    bool MemoryPool::shrink(closed_iter p_iter, const size_t &N) {
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
                    if (isAligned(type_size, right.head, b.head, b.head_size)) {
                        right.head = b.head;
                        right.head_size += b.head_size;
                        if (!update(OpenList, find_open(right), right)) {
                            throw failed_operation(__CEFUNCTION__, __LINE__, "Failed to find OpenList iterator.");
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

/*
*/
    void MemoryPool::add_open(const alloc &a) {
        auto result = OpenAllocations.emplace(a.head, a);
        if (!result.second) {
            throw bad_request(__CEFUNCTION__, __LINE__, "Allocation already exists in OpenAllocations");
        }
        // todo: check for pre-existing allocation in another way
        if (!OpenList.emplace(a.head_size, result.first)) {
            throw bad_request(__CEFUNCTION__, __LINE__, "Allocation already exists in OpenList");
        }
    }

/*
*/
    void MemoryPool::erase_open(openlist_iter iter) {
        if (iter == OpenList.end()) {
            throw invalid_args(__CEFUNCTION__, __LINE__);
        }
        OpenAllocations.erase(iter->second);
        OpenList.erase(iter);
    }

/*
*/
    void MemoryPool::erase_open(openalloc_iter iter) {
        if (iter == OpenAllocations.end()) {
            throw invalid_args(__CEFUNCTION__, __LINE__);
        }
        alloc a = iter->second;
        auto iter2 = find_open(a);
        if (iter2 == OpenList.end()) {
            throw bad_request(__CEFUNCTION__, __LINE__, "Cannot find the OpenList iterator");
        }
        OpenAllocations.erase(iter);
        OpenList.erase(iter2);
    }

/* protected method
    moves an allocation from the ClosedList to the OpenList
*/
    void MemoryPool::moveto_open(closed_iter iter) {
        //todo: perform merging
        if (iter == ClosedList.end()) {
            throw invalid_args(__CEFUNCTION__, __LINE__, "Iterator is invalid.");
        }

        alloc &p_alloc = iter->second;
        m_free += p_alloc.head_size;
        m_used -= p_alloc.head_size;
        auto neighbours = find_neighbours(p_alloc.head);
        alloc left, right;
        if (neighbours.second != OpenAllocations.end()) {
            right = neighbours.second->second;
        }
        if (neighbours.first != OpenAllocations.end()) {
            left = neighbours.first->second;
        }

        //todo: remember we might need to merge with both left and right
        bool merged = false;
        if (isAligned(type_size, right.head, p_alloc.head, p_alloc.head_size)) {
            auto iter = find_open(right);
            merged = merge(iter, p_alloc);
            p_alloc = iter->second->second;
            if (!merged) {
                throw failed_operation(__CEFUNCTION__, __LINE__, "Failed attempt of merging allocations.");
            }
        }
        if (isAligned(type_size, p_alloc.head, left.head, left.head_size)) {
            auto left_ol_iter = find_open(left);
            if (merged) {
                auto right_ol_iter = find_open(right);
                merged = merge(left_ol_iter, p_alloc);
                erase_open(right_ol_iter);
            } else {
                merged = merge(left_ol_iter, p_alloc);
            }
            if (!merged) {
                throw failed_operation(__CEFUNCTION__, __LINE__, "Failed attempt of merging allocations.");
            }
        }
        if (!merged) {
            add_open(p_alloc);
        }
        ClosedList.erase(iter);
    }

/* protected method
    moves an allocation from the ClosedList to the OpenList
*/
    void MemoryPool::moveto_open(closed_iter iter, const size_t &N) {
        if (iter == ClosedList.end()) {
            throw invalid_args(__CEFUNCTION__, __LINE__, "Iterator is invalid.");
        }
        alloc &a = iter->second;
        if (a.head == p && a.head_size == N) {
            moveto_open(iter);
        } else if (inRange(type_size, p, N, a.head, a.head_size)) {
            auto neighbours = find_neighbours(p_alloc.head);
            alloc left, right;
            if (neighbours.second != OpenAllocations.end()) {
                right = neighbours.second->second;
            }
            if (neighbours.first != OpenAllocations.end()) {
                left = neighbours.first->second;
            }

            alloc b{a.head, p, a.master_size, N};
            m_free += b.head_size;
            m_used -= b.head_size;
            if (isAligned(type_size, right.head, b.head, b.head_size)) {
                if (!merge(find_open(right), b)) {
                    throw failed_operation(__CEFUNCTION__, __LINE__, "Failed attempt of merging allocations.");
                }
                a.head_size -= N;
            } else if (isAligned(type_size, b.head, left.head, left.head_size)) {
                if (!merge(find_open(left), b)) {
                    throw failed_operation(__CEFUNCTION__, __LINE__, "Failed attempt of merging allocations.");
                }
                a.head += N;
                a.head_size -= N;
            } else {
                //update front closed, add back closed, add middle to open
                size_t front = b.head - a.head;
                alloc c{b.master, b.head + b.head_size, b.master_size, a.head_size - (b.head_size + front)}
                a.head_size = front;
                add_open(b);
                ClosedList.emplace(c.head, c);
            }
            update(ClosedList, iter, a);

        } else {
            throw bad_request(__CEFUNCTION__, __LINE__, "Range [p,p+N] is not in the range of the iterator sub-allocation passed.");
        }
    }

/*
*/
    neighbours MemoryPool::find_neighbours(const uintptr_t &p) {
        //assert(isClosed(p) && "p isn't managed, what did you do!"); //maybe an exception should be thrown :-/
        auto end = OpenAllocations.end();
        if (isOnOpened(p)) {
            // p would have been merged with its neighbours when joining the Open list
            return std::make_pair(end, end); //throwing an exception is probably preferable
        }

        openalloc_iter left, right;
        left = right = OpenAllocations.lower_bound(p);
        if (left != OpenAllocations.begin()) {
            --left;
        } else {
            left = end;
        }
        auto iter = find_closed(p);
        alloc &a = iter->second;
        if (left != end) {
            if (!isAligned(type_size, a.head, left->second.head, left->second.head_size)) {
                left = end;
            }
        }
        if (right != end) {
            if (!isAligned(type_size, right->second.head, a.head, a.head_size)) {
                right = end;
            }
        }
        return std::make_pair(left, right);
    }

/* todo: revise?
*/
    closed_iter MemoryPool::find_closed(const uintptr_t &p) {
        auto iter = ClosedList.lower_bound(p);
        if (iter != ClosedList.end()) {
            if (iter->second.head != p) {
                if (iter != ClosedList.begin()) {
                    --iter;
                    // todo: figure out inRange math, and what we're missing for the call (look for other usages, that don't have errors)
                    if (!inRange(p, iter->second.head, iter->second.head_size)) {
                        iter = ClosedList.end()
                    }
                } else {
                    iter = ClosedList.end();
                }
            }
        }
        return iter;
    }

/*
*/
    openlist_iter MemoryPool::find_open(const alloc &a) {
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

    openalloc_iter MemoryPool::find_open(const uintptr_t &p) {
        auto iter = OpenAllocations.lower_bound(p);
        if (iter != OpenAllocations.end()) {
            if (iter->second.head != p) {
                if (iter != OpenAllocations.begin()) {
                    --iter;
                    if (!inRange(p, iter->second.head, iter->second.head_size)) {
                        iter = OpenAllocations.end()
                    }
                } else {
                    iter = OpenAllocations.end();
                }
            }
        }
        return iter;
    }
}
