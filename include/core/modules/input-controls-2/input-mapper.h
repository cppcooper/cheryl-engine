#pragma once
#include<vector>
#include "InputContext.h"

/* Maps input for a particular user, I think. 99% sure. */
class InputBinding;
class InputMapper
{
    friend class InputSystem;
private:
    InputContext m_Context;
    std::vector<InputBinding*> m_Bindings;
public:
    void Poll();
};