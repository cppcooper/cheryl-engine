#include "singleton.h"
#include <iostream>

namespace detail{
    void print_already_constructed_error(){
        std::cerr << "Already constructed Singleton<" << type_name << ">\n";
    }
}