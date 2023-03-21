#pragma once

#include <bitset>

/* For ensuring input is of a particular user?? (incomplete implementation if so)
I think, one of those times I wish I finished.. or made comments*/
union InputContext
{
    unsigned int value;
    #ifdef _MSC_VER
    struct{
        std::bitset<32> mask;
    };
    #else
    std::bitset<32> mask;
    #endif

    InputContext();
    bool operator==( const InputContext& other );
    bool operator!=( const InputContext& other );
};