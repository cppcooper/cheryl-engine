#include <resource-management/memory.h>
#include <cstdlib>
using namespace CherylE;

/* protected method
    allocates the bytes rounded to the next power of two
*/
alloc MemoryMgr::allocate(size_t &bytes) {
    bytes = nextPowerof2(bytes);
    void* p = malloc(bytes);
    if(!ptr){
        throw bad_alloc(__FUNCTION__,__LINE__);
    } else if(!MasterRecord.emplace(p).second){
        throw failed_operation(__FUNCTION__, __LINE__, "New allocation is already recorded. This should never happen, something went wrong.");
    }
    m_total += bytes;
    m_free += bytes;
    return alloc{p,p,bytes,bytes};
}

/* protected method
    moves an allocation from the ClosedList to the OpenList
*/
void MemoryMgr::open_alloc(closed_iter iter, size_t bytes){
    alloc a = iter->second;
    size_t remainder = 0;
    if(bytes <= a.head_size){
        remainder = a.head_size - bytes;
        a.head_size = bytes;
        m_used -= bytes;
        m_free += bytes;
    } else {
        throw invalid_args(__FUNCTION__, __LINE__, "The bytes returned must be less than or equal to the bytes allocated.");
    }
    ClosedList.erase(iter);
    OpenList.emplace(bytes,a);
    if(remainder>0){
        a.head += a.head_size;
        a.head_size = remainder;
        ClosedList.emplace(a.master,a);
    }
}

/* protected method (searches ClosedList)
    attempts to find the iterator of the allocation p belongs to
    returns an iterator to end() if search fails
*/
closed_iter MemoryMgr::find(void* p){
    /* Get first pair with a key not less than p
    -p is a masterptr
        search will land directly on the right pair
    -p is not a master pointer
        -p is not the head of an allocation
            must find the head
        -p is the head of an allocation
            must match p to pair.second.head
    */
    if(!ClosedList.empty()){
        auto iter = ClosedList.find(p);
        if(iter!=ClosedList.end()){
            //we found an exact match
            return iter;
        }
        iter = ClosedList.lower_bound(p);
        if(iter!=ClosedList.begin()){
            //we found something not less than what we're looking for
            iter--;
            void* mptr = iter->first; //p should mptr < p < mptr+master_size
            if(mptr < p && p < mptr+iter->second.master_size){
                //we just need to do a linear search now to find the right sub-allocation
                while(true){
                    if(mptr != iter->first){
                        //we've gone too far
                        return ClosedList.end();
                    }
                    alloc a = iter->second;
                    if(p == a.head){
                        return iter;
                    } else if (a.head < p && p < a.head+a.head_size){
                        //we found the sub-allocation that p is a sub-allocation of
                        return iter;
                    }
                    if(iter==ClosedList.begin()){
                        break;
                    }
                    iter--;
                }
            }
        }
    }
    return ClosedList.end();
}

/* prepares N blocks of M sized memory allocations
    calling the method(M,N)
*/
void MemoryMgr::pre_allocate(size_t bytes, size_t blocks) {
    if (blocks < 1){
        throw invalid_args(__FUNCTION__, __LINE__, "The number of allocated blocks cannot be less than 1.");
    }
    for(int i = 0; i < blocks; ++i){
        alloc a = allocate(bytes);
        OpenList.emplace(bytes,a);
    }
}

/* returns an allocation of the required size or greater
    searches for an allocation according to the desired fit type (best/worst)
    if none are found, it creates one
*/
void* MemoryMgr::get(size_t bytes, fitType fit) {
    if (bytes < 1){
        throw invalid_args(__FUNCTION__, __LINE__, "The number of bytes must be at least 1.");
    }
    alloc a;
    size_t remainder = 0;
    //get an allocation {best fit}{worst fit}
    if (fit == fitType::bestFit){
        auto iter = OpenList.lower_bound(bytes); //best fit
        if(iter!=OpenList.end()){
            a = iter->second;
            OpenList.erase(iter);
        }else{
            a = allocate(bytes);
        }
    } else if (fit == fitType::worstFit){
        auto iter = OpenList.rbegin(); //worst fit
        if(iter!=OpenList.rend()){
            a = iter->second;
            OpenList.erase(iter);
        }else{
            a = allocate(bytes);
        }
    }
    remainder = bytes - a.head_size;
    a.head_size -= remainder;
    m_used += bytes;
    m_free -= bytes;
    ClosedList.emplace(a.master,a);
    if(remainder>0){
        a.head += a.head_size;
        a.head_size = remainder;
        OpenList.emplace(a.master,a);
    }
}


/* attempts to put back an allocation
    searches the closed list for an allocation range wherein p belongs
    if a valid range is found p to head+head_size is returned to the available memory
*/
void MemoryMgr::put(void* p){
    auto iter = find(p);
    if(iter!=ClosedList.end()){
        alloc a = iter->second;
        if(p <= a.head+a.head_size-1){
            if(p==a.head){
                open_alloc(iter);
                return;
            }
            size_t front_size = p-a.head;
            size_t back_size = a.head_size-front_size;
            iter->second.head_size = front_size;
            a.head = p;
            a.head_size = back_size;
            m_used -= back_size;
            m_free += back_size;
            OpenList.emplace(back_size,a);
            return;
        }
        throw failed_operation(__FUNCTION__, __LINE__, "This pointer cannot be returned.");
    }
    throw failed_operation(__FUNCTION__, __LINE__, "Pointer does not belong to any known allocation.");
}

/* attempts to put back an allocation
    searches the closed list for an allocation range wherein p belongs
    if a valid range is found p to p+bytes is returned to the available memory
*/
void MemoryMgr::put(void* p, size_t bytes) {
    auto iter = find(p);
    if(iter!=ClosedList.end()){
        alloc a = iter->second;
        if(p+bytes <= a.head+a.head_size){
            if(p==a.head){
                open_alloc(iter);
                return;
            }
            size_t front_size = p-a.head;
            size_t back_size = a.head_size-front_size-bytes;
            iter->second.head_size = front_size;
            a.head = p;
            a.head_size = bytes;
            m_used -= bytes;
            m_free += bytes;
            OpenList.emplace(bytes,a);
            if(back_size>0){
                a.head += a.head_size;
                a.head_size = back_size;
                ClosedList.emplace(a.master,a);
            }
            return;
        }
        throw failed_operation(__FUNCTION__, __LINE__, "The bytes returned would exceed the allocation size.");
    }
    throw failed_operation(__FUNCTION__, __LINE__, "Pointer does not belong to any known allocation.");
}
