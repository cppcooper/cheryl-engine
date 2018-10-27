#include <resource-management/memory.h>
#include <cstdlib>
using namespace CherylE;

void* MemoryMgr::allocate(size_t bytes) {
    void* p = malloc(bytes);
    if(!ptr){
        throw bad_alloc(__FUNCTION__,__LINE__);
    }
    return p;
}

void MemoryMgr::pre_allocate(size_t bytes, size_t blocks) {
    if (blocks < 1){
        throw invalid_args(__FUNCTION__, __LINE__, "The number of allocated blocks cannot be less than 1.");
    }
    for(int i = 0; i < blocks; ++i){
        alloc a;
        a.master = allocate(bytes);
        a.head = a.master;
        a.master_size = bytes;
        a.head_size = bytes;
        if(!MasterRecord.emplace(p).second){
            throw failed_operation(__FUNCTION__, __LINE__, "New allocation is already recorded. This should never happen, something went wrong.");
        }
        OpenList.emplace(bytes,a);
    }
}

void* MemoryMgr::get(size_t bytes, fitType fit) {
    if (fit == fitType::bestFit){
        auto iter = OpenList.lower_bound(bytes); //best fit
    } else if (fit == fitType::worstFit){
        auto iter = OpenList.upper_bound(bytes);
    }
}

void MemoryMgr::put(void *p, size_t bytes) {

}

