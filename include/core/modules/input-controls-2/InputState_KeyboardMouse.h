#pragma once
#include "InputState.h"
#include <Windows.h>

class InputStateDigital_KeyboardMouse : public InputStateDigital
{
private:
    int m_VKey = 0;

public:
    //Virtual-Key Codes https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx
    InputStateDigital_KeyboardMouse( int VK_CODE );
    bool poll() final override;
};

class InputStateAnalog_Mouse : public InputStateAnalog
{
public:
    void poll( double& x, double& y ) final override;
};