#include "../InputSystem.h"
#include "../InputMapper.h"

void InputSystem::Poll()
{
    for ( auto mapper : m_Mappers )
    {
        if ( m_ActiveContext == mapper->m_Context )
        {
            mapper->Poll();
        }
    }
}

void InputSystem::Init()
{
    //m_ActiveControllers = SteamController()->GetConnectedControllers( m_SteamControllers );
}

void InputSystem::Process()
{

}