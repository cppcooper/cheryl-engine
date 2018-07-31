#include "singleton.h"
#include <iostream>

namespace detail{
    void print_already_constructed_error(const char* type_name){
        std::cerr << "Already constructed Singleton<" << type_name << ">\n";
    }
}