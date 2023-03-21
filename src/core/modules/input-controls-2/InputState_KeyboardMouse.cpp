#include "../InputState_KeyboardMouse.h"

InputStateDigital_KeyboardMouse::InputStateDigital_KeyboardMouse( int VK_CODE )
{
    m_VKey = VK_CODE;
}

bool InputStateDigital_KeyboardMouse::poll()
{
    return GetAsyncKeyState( m_VKey );
}

void InputStateAnalog_Mouse::poll( double & x, double & y )
{
    POINT Cursor;
    GetCursorPos( &Cursor );
    x = (double)Cursor.x;
    y = (double)Cursor.y;
}
