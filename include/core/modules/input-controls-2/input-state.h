#pragma once
#include <string>

class InputStateDigital
{
public:
    virtual bool poll() = 0;
};

class InputStateAnalog
{
public:
    virtual void poll( double& x, double& y ) = 0;
};