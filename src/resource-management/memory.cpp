#include <resource-management/memory.h>
#include <cstdlib>
using namespace CherylE;

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
alloc MemoryMgr::allocate(const size_t &bytes) {
    if(bytes < 1){
        throw invalid_args(__CEFUNCTION__, __LINE__, "The number of allocated blocks cannot be less than 1.");
    }
    bytes = nextPowerof2(bytes);
    void* ptr = malloc(bytes);
    if(!ptr){
        throw bad_alloc(__CEFUNCTION__,__LINE__);
    }
    ce_uintptr p = (ce_uintptr)ptr;
    MasterRecord.emplace(p);
    m_total += bytes;
    m_free += bytes;
    return alloc{p,p,bytes,bytes};
}

