#pragma once
#ifndef CEGAMEMODULE_H_
#define CEGAMEMODULE_H_

namespace CherylE{
    class GameModule{
    public:
        virtual void Init() {}
        virtual void Deinit() {}

        virtual void Process() {}
        virtual void Update(double& seconds) {}
        virtual void Buffer() {}

        virtual void Draw() {} //Todo: Replace with below method
        virtual void Output() {}
    };
}

#endif