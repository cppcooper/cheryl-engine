#pragma once
#ifndef CEPOOL_H
#define CEPOOL_H

#include <abstracts/pool.h>
#include <abstracts/singleton.h>

namespace CherylE {

    // As a child of VirtualSingleton, this class can't be instantiated

    template<class T>
    class Pool : public DerivedSingleton<MemoryPool>{
    TYPENAMEAVAILABLE_VIRTUAL
        //remove iPool, duplicate interface with T*
        static_assert(isclass(T), "In iPoolT<T>, T must be a class.");
        // todo: introduce builder class or some such concept so that trivial constructors are not required
        static_assert(hasdefconstructor(T), "In iPoolT<T>, T must be default trivially constructible.");
        static_assert(hasmethod(T, TypeName), "In iPoolT<T>, T must implement the method TypeName");
        static_assert(hasmethod(T, get_typeID), "In iPoolT<T>, T must implement the method typeID");
    public:
        // todo: shouldn't this be implemented here?
        /*returns an array of size N*/
        virtual T* getT(size_t N, fitType fit/* = fitType::worstFit*/) = 0;
    };
}

#endif
