// Existing negative-test allocation override, isolated from optimized callers.
// Linked only into the three executables that already used this fixture.
#include "allocation-denial.h"
#include <cstdlib>
#include <new>

namespace ProcGenTestAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t size) {
    if (ProcGenTestAllocation::denied) throw std::bad_alloc();
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
