#pragma once
#ifndef _ASSET_H
#define _ASSET_H

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