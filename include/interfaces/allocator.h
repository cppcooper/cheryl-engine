#pragma once
#ifndef CEALLOCATOR_H
#define CEALLOCATOR_H

namespace CherylE{
    class iAllocator{
    public:
        virtual ~iAllocator();
        virtual void* allocate(size_t) = 0;
        virtual void deallocate(void*, size_t) = 0;
    };
}

#endif