#include "../InputContext.h"

InputContext::InputContext()
{
    value = 0;
}
bool InputContext::operator==( const InputContext& other )
{
    return value & other.value;
}
bool InputContext::operator!=( const InputContext& other )
{
    return ( value & other.value ) == 0;
}