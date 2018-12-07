#include <resource-management/memory.h>
#include <cstdlib>
using namespace CherylE;

/* class destructor
    frees allocations
*/
MemoryMgr::~MemoryMgr(){
    purge();
}

/* frees all allocations
*/
void MemoryMgr::purge(){
    for(auto p : MasterRecord){
        free(p);
    }
    MasterRecord.clear();
    OpenList.clear();
    ClosedList.clear();
    m_free = 0;
    m_used = 0;
    m_total = 0;
}

/* protected method
    allocates the bytes rounded to the next power of two
*/
alloc MemoryMgr::allocate(size_t &bytes) {
    if(bytes < 1){
        throw invalid_args(__CEFUNCTION__, __LINE__, "The number of allocated blocks cannot be less than 1.");
    }
    bytes = nextPowerof2(bytes);
    void* p = malloc(bytes);
    if(!ptr){
        throw bad_alloc(__CEFUNCTION__,__LINE__);
    } else if(!MasterRecord.emplace(p).second){
        throw failed_operation(__CEFUNCTION__, __LINE__, "New allocation is already recorded. This should never happen, something went wrong.");
    }
    m_total += bytes;
    m_free += bytes;
    return alloc{p,p,bytes,bytes};
}

/* prepares N blocks of M sized memory allocations
    calling the method(M,N)
*/
void MemoryMgr::pre_allocate(size_t bytes, size_t blocks) {
    if(blocks < 1){
        throw invalid_args(__CEFUNCTION__, __LINE__, "The number of allocated blocks cannot be less than 1.");
    }
    for(int i = 0; i < blocks; ++i){
        alloc a = allocate(bytes);
        add_open(a);
    }
}


/* returns an allocation of the required size or greater
    searches for an allocation according to the desired fit type (best/worst)
    if none are found, it creates one
*/
void* MemoryMgr::get(size_t bytes, fitType fit) {
    if (bytes < 1){
        throw invalid_args(__CEFUNCTION__, __LINE__, "The number of bytes must be at least 1.");
    }
    alloc a;
    size_t remainder = 0;
    //get an allocation {best fit}{worst fit}
    if (fit == fitType::bestFit){
        auto iter = OpenList.lower_bound(bytes); //best fit
        if(iter!=OpenList.end()){
            a = iter->second->second;
            erase_open(iter);
        }else{
            remainder = bytes;
            a = allocate(bytes);
        }
    } else if (fit == fitType::worstFit){
        auto iter = OpenList.rbegin(); //worst fit
        if(iter!=OpenList.rend()){
            a = iter->second->second;
            erase_open(iter);
        }else{
            remainder = bytes;
            a = allocate(bytes);
        }
    }
    //if remainder != 0, allocated bytes may be larger than what was requested
    remainder = remainder != 0 ? bytes - remainder : bytes - a.head_size;
    a.head_size = bytes - remainder;
    m_used += bytes;
    m_free -= bytes;
    ClosedList.emplace(a.master,a);
    if(remainder>0){
        a.head += a.head_size;
        a.head_size = remainder;
        add_open(a);
    }
}

/* returns the allocation size of p
    p does not need to be the head
*/
size_t MemoryMgr::size(void* p){
    auto iter = find_closed(p);
    if(iter!=ClosedList.end()){
        alloc a = iter->second;
        if(p==a.head){
            return a.head_size;
        }
        size_t front_size = p-a.head;
        if(a.head_size > front_size){
            return a.head_size-front_size;
        }
        throw failed_operation(__CEFUNCTION__, __LINE__, "Size underflow. It is unclear why this would ever happen.");
    }
    return 0;
}

/* attempts to resize allocation of p
    returns false on fail, true otherwise
*/
resizeResult MemoryMgr::resize(void* &p, size_t bytes, bool allow_realloc){
    if(bytes < 1){
        throw invalid_args(__CEFUNCTION__, __LINE__, "Cannot resize allocation to less than 1 byte.");
    }
    auto p_iter = find_closed(p);
    if(p_iter != ClosedList.end()){
        auto neighbours = find_neighbours(p);
        alloc a = p_iter->second;
        if(bytes==a.head_size){
            throw bad_request(__CEFUNCTION__, __LINE__, "New size is equal to current size.");
        }

        if(bytes>a.head_size){
            size_t bytes_needed = bytes-a.head_size;
            alloc left,right;
            if(neighbours.second != OpenAllocations.end()){
                right = neighbours.second.second;                
            }
            if(neighbours.first != OpenAllocations.end()){
                left = neighbours.first.second;
            }
            //check right side then left side
            if(a.head+a.head_size == right.head && bytes_needed <= right.head_size) {
                //right side has enough space
                //& right_alloc aligns with end of p_alloc
                auto right_ol_iter = find_open(right);
                if(right_ol_iter==OpenList.end()){
                    throw failed_operation(__CEFUNCTION__, __LINE__, "Couldn't find iterator for right-side allocation.");
                }
                right.head_size -= bytes_needed;
                if(right.head_size==0){
                    //right_alloc has been used up, time to take it out of the OpenAllocations
                    erase_open(right_ol_iter);
                } else {
                    //update iterators (key: <size_t,iter>) (value: <ptr,alloc>)
                    neighbours.second.head_size = right.head_size; //allocation updated
                    right_ol_iter->first = right.head_size; //available size updated
                }
                p_iter->second.head_size = bytes;
                m_free -= bytes_needed;
                m_used += bytes_needed;
                return resizeResult::right;
            } else if(p == left.head+left.head_size && bytes_needed <= left.head_size) {
                //left side has enough space
                //& p_alloc aligns with end of left_alloc
                auto left_ol_iter = find_open(left);
                if(left_ol_iter==OpenList.end()){
                    throw failed_operation(__CEFUNCTION__, __LINE__, "Couldn't find iterator for left-side allocation.");
                }
                left.head_size -= bytes_needed;
                if(left.head_size==0){
                    //left_alloc has been used up, time to take it out of the OpenAllocations
                    erase_open(left_ol_iter);
                } else {
                    //update iterators (key: <size_t,iter>) (value: <ptr,alloc>)
                    neighbours.second.head_size = left.head_size;
                    left_ol_iter->first = left.head_size;
                }
                p_iter->second.head_size = bytes;
                m_free -= bytes_needed;
                m_used += bytes_needed;
                return resizeResult::left;
            } else {
                /** We don't use both sides in conjunction, because each side might not be equal
                * So the only way to be able to easily handle those situations would be to balance usage
                * between left and right. Then we'd have to deal with odd numbers, and the logic would be
                * overly complex. It is easier to reallocate in fact.
                */
            }
            return resizeResult::fail;
        } else {
            //shrink
            auto right_iter = OpenAllocations.upper_bound(p);
            alloc right = right_iter->second;
            size_t bytes_returned = a.head_size-bytes;

            if(a.head+a.head_size==right.head){
                auto right_ol_iter = find_open(right);
                if(right_ol_iter==OpenList.end()){
                    throw failed_operation(__CEFUNCTION__, __LINE__, "Couldn't find iterator for right-side allocation.");
                }
                right_iter->second.head -= bytes_returned;
                right_iter->second.head_size += bytes_returned;
                right_ol_iter->first = right_iter->second.head_size;
            }
            m_free += bytes_returned;
            m_used -= bytes_returned;
            return resizeResult::right;
        }
    } else {
        throw failed_operation(__CEFUNCTION__, __LINE__, "Pointer does not belong to any known allocation.");
    }
    return resizeResult::fail;
}

/* attempts to put back an allocation
    searches the closed list for an allocation range wherein p belongs
    if a valid range is found p to head+head_size is returned to the available memory
*/
void MemoryMgr::put(void* p){
    auto iter = find_closed(p);
    if(iter!=ClosedList.end()){
        alloc a = iter->second;
        if(p==a.head){
            moveto_open(iter);
            return;
        } else if(p <= a.head+a.head_size) {
            alloc b{a.master, p, a.master_size, a.head+a.head_size-p};
            iter->second.head_size = p-a.head;
            add_open(b);
            m_free += b.head_size;
            m_used -= b.head_size;
            return;
        }
        throw failed_operation(__CEFUNCTION__, __LINE__, "This pointer cannot be returned.");
    }
    throw failed_operation(__CEFUNCTION__, __LINE__, "Pointer does not belong to any known allocation.");
}

/* attempts to put back an allocation
    searches the closed list for an allocation range wherein p belongs
    if a valid range is found p to p+bytes is returned to the available memory
*/
void MemoryMgr::put(void* p, size_t bytes) {
    auto iter = find_closed(p);
    if(iter!=ClosedList.end()){
        alloc a = iter->second;
        if(p==a.head && bytes==a.head_size){
            moveto_open(iter);
            return;
        }
        //todo: write method
        /*
        check left only  <==  remainder>0 && p == head
        check right only <==  p-head == remainder
        check nothing    <==  p-head != (0 || remainder)
        */
        size_t offset = p - a.head;
        size_t remainder = a.head_size - bytes;
        if(remainder==0){
            //check both sides of allocation for open entries

        } else if(offset != 0 && remainder != 0 && offset != remainder){
            //results in two closed list entries
            iter->second.head_size = p-a.head_size;
        } else if(offset==0) {
            //check left side of allocation for an open entry
        } else if(offset==remainder) {
            //check right side of allocation for an open entry  
        }
        throw failed_operation(__CEFUNCTION__, __LINE__, "The bytes returned would exceed the allocation size.");
    }
    throw failed_operation(__CEFUNCTION__, __LINE__, "Pointer does not belong to any known allocation.");
}
