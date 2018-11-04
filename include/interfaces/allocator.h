#pragma once
#ifndef ALLOCATOR_H
#define ALLOCATOR_H

namespace CherylE{
    class iAllocator{
    public:
        virtual ~iAllocator();
        virtual void* allocate(size_t) = 0;
        virtual void deallocate(void*, size_t) = 0;
    };
}

#endif