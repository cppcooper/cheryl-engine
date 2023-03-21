#include "../InputState_SteamController.h"
#include <assert.h>
/*
InputStateDigital_SteamController::InputStateDigital_SteamController( ControllerHandle_t activeControllerHandle, std::string action )
{
    static auto SC = SteamController();
    m_ActiveControllerHandle = activeControllerHandle;
    m_ActionHandle = SC->GetDigitalActionHandle( action.c_str() );
    assert( m_ActiveControllerHandle && m_ActionHandle );
}

bool InputStateDigital_SteamController::poll()
{
    static auto SC = SteamController();
    if ( m_ActionHandle != 0 )
    {
        ControllerDigitalActionData_t digitalData = SC->GetDigitalActionData( m_ActiveControllerHandle, m_ActionHandle );
        if ( digitalData.bActive )
            return digitalData.bState;
    }
    return false;
}

InputStateAnalog_SteamController::InputStateAnalog_SteamController( ControllerHandle_t activeControllerHandle, std::string action )
{
    static auto SC = SteamController();
    m_ActiveControllerHandle = activeControllerHandle;
    m_ActionHandle = SC->GetAnalogActionHandle( action.c_str() );
}

void InputStateAnalog_SteamController::poll( double & x, double & y )
{
    static auto SC = SteamController();
    if ( m_ActionHandle != 0 )
    {
        ControllerAnalogActionData_t analogData = SC->GetAnalogActionData( m_ActiveControllerHandle, m_ActionHandle );
        if ( analogData.bActive )
        {
            x = analogData.x;
            y = analogData.y;
        }
        else
        {
            x = 0;
            y = 0;
        }
    }
}
*/