#include <resource-management/memory.h>
#include <cstdlib>
using namespace CherylE;

alloc MemoryMgr::allocate(size_t bytes) {
    void* p = malloc(bytes);
    if(!ptr){
        throw bad_alloc(__FUNCTION__,__LINE__);
    } else if(!MasterRecord.emplace(p).second){
        throw failed_operation(__FUNCTION__, __LINE__, "New allocation is already recorded. This should never happen, something went wrong.");
    }
    return alloc{p,p,bytes,bytes};
}

void MemoryMgr::pre_allocate(size_t bytes, size_t blocks) {
    if (blocks < 1){
        throw invalid_args(__FUNCTION__, __LINE__, "The number of allocated blocks cannot be less than 1.");
    }
    bytes = nextPowerof2(bytes);
    for(int i = 0; i < blocks; ++i){
        alloc a = allocate(bytes);
        OpenList.emplace(bytes,a);
    }
}

void* MemoryMgr::get(size_t bytes, fitType fit) {
    if (bytes < 1){
        throw invalid_args(__FUNCTION__, __LINE__, "The number of bytes must be at least 1.");
    }
    alloc a;
    size_t remainder = 0;
    bytes = nextPowerof2(bytes);
    //get an allocation {best fit}{worst fit}
    if (fit == fitType::bestFit){
        auto iter = OpenList.lower_bound(bytes); //best fit
        if(iter!=OpenList.end()){
            a = iter->second;
        }else{
            a = allocate(bytes);
        }
    } else if (fit == fitType::worstFit){
        auto iter = OpenList.rbegin(); //worst fit
        if(iter!=OpenList.rend()){
            a = iter->second;
        }else{
            a = allocate(bytes);
        }
    }
    remainder = bytes - a.head_size;
    a.head_size -= remainder;
    ClosedList.emplace(a.master,a);
    if(remainder>0){
        a.head += a.head_size;
        a.head_size = remainder;
        OpenList.emplace(a.master,a);
    }
}

void MemoryMgr::put(void *p, size_t bytes) {
}

