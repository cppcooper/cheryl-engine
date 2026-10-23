#include "console.h"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    // Opt-in transcript instrumentation; never included in the save envelope.
    const bool trace = argc == 2 && std::string_view(argv[1]) == "--trace-effects";
    if (argc != 1 && !trace) {
        std::cerr << "Usage: dialogue-console [--trace-effects]\n";
        return 2;
    }
    try {
        std::size_t effects = 0;
        GateConsole::Encounter encounter(trace ? &effects : nullptr);
        const auto result = GateConsole::run_console(encounter, std::cin, std::cout);
        if (trace)
            std::cerr << "Introduce effects: " << effects << '\n';
        return result;
    } catch (const std::exception& failure) {
        std::cerr << "Encounter failure: " << failure.what() << '\n';
        return 1;
    }
}
