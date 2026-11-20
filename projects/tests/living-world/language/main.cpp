#include "practice.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        return CE::LivingWorld::Language::run_practice(std::cin, std::cout);
    } catch (const std::exception&) {
        std::cerr << "Practice session stopped; no casting engine is connected.\n";
        return 1;
    }
}
