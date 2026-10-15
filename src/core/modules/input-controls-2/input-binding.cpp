#include "../InputBinding.h"
#include "../InputState.h"

InputBindingDigital::InputBindingDigital( InputStateDigital * state, std::function<void()> callback, unsigned int priority )
{
    m_State = state;
    m_ActionCallback = callback;
    m_Priority = priority;
}

void InputBindingDigital::poll()
{
    if ( m_State->poll() )
        m_ActionCallback(); //This should be queued
}


InputBindingAnalog::InputBindingAnalog( InputStateAnalog * state, std::function<void( double, double )> callback, unsigned int priority )
{
    m_State = state;
    m_RangeCallback = callback;
    m_Priority = priority;
}

void InputBindingAnalog::poll()
{
    double x;
    double y;
    m_State->poll( x, y );
    m_RangeCallback( x, y ); //This should be queued
}
