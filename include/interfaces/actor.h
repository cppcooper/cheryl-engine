#pragma once
#ifndef CEACTOR_H
#define CEACTOR_H

namespace CherylE{
    class iActor{
    public:
        virtual ~iActor();
        virtual void Reset() = 0;
        virtual void Save() = 0;
        virtual void Load(char* data) = 0;
        virtual void Unload() = 0;
    };
}

#endif