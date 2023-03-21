#include "../InputMapper.h"
#include "../InputBinding.h"

void InputMapper::Poll()
{
    for ( auto binding : m_Bindings )
    {
        binding->poll();
    }
}
