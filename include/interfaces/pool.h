#pragma once
#ifndef CEPOOL_H
#define CEPOOL_H
#include <stdafx.h>
#include "../abstracts/pool.h"

namespace CherylE{
    template<class typeinterface>
    class iPoolT : public AbstractPool{ //remove iPool, duplicate interface with T*
        static_assert(isclass(typeinterface),"In iPoolT<T>, T must be a class.");
        static_assert(hasdefconstructor(T), "In iPoolT<T>, T must be default trivially constructible.");
        static_assert(hasmethod(T,TypeName),"In iPoolT<T>, T must implement the method TypeName");
        static_assert(hasmethod(T,get_typeID),"In iPoolT<T>, T must implement the method typeID");
    public:
        /*returns an array of size N*/
        virtual typeinterface* getT(size_t N, const fitType fit/* = fitType::worstFit*/) = 0;
    };
}

#endif