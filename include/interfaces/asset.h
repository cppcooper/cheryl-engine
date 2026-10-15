#pragma once
#ifndef CEASSET_H
#define CEASSET_H

namespace CherylE{
    class iAsset{
    protected:
        size_t typeID = 0;
    public:
        virtual ~iAsset();
        virtual size_t get_typeID()const{ return typeID; }
        virtual const char* TypeName() = 0;
        virtual void reset() = 0;
        virtual void load(const char* data) = 0;
        virtual void unload() = 0;
    };
}

#endif