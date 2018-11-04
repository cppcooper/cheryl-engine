#pragma once
#ifndef CEASSET_H
#define CEASSET_H

namespace CherylE{
    class iAsset{
    public:
        virtual ~iAsset();
        virtual void Reset() = 0;
        virtual void Load(char* data) = 0;
        virtual void Unload() = 0;
    };
}

#endif