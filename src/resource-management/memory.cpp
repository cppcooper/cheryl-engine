#include <resource-management/memory.h>
#include <cstdlib>
using namespace CherylE;

void* MemoryMgr::get(size_t bytes) {
    //todo: check for available allocation
    void* ptr = malloc(bytes);
    if (ptr == nullptr) {
        throw base_exception("replace with bad_alloc");
    }
    //todo: stash allocation remainder
    return ptr;
}
void MemoryMgr::put(void *ptr, size_t bytes) {

}

uint64_t MemoryMgr::length(){
    return 0;
}

uint64_t MemoryMgr::size(){
    return 0;
}