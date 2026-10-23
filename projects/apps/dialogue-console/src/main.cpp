#include "console.h"
#include "definition-codec.h"

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
    // Opt-in transcript instrumentation; never included in the save envelope.
    bool trace = false;
    std::optional<std::string> path;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--trace-effects" && !trace)
            trace = true;
        else if (argument == "--definition" && !path && index + 1 < argc && !std::string_view(argv[index + 1]).starts_with("--"))
            path = argv[++index];
        else {
            std::cerr << "Usage: dialogue-console [--definition PATH] [--trace-effects]\n";
            return 2;
        }
    }
    try {
        GateConsole::D::DefinitionHandle definition;
        if (path) {
            auto loaded = GateConsole::load_definition(*path);
            if (!loaded) {
                std::cerr << GateConsole::format_definition_error(loaded.error()) << '\n';
                return 3;
            }
            definition = std::move(*loaded);
        } else
            definition = GateConsole::gate_definition();
        std::size_t effects = 0;
        GateConsole::Encounter encounter(std::move(definition), trace ? &effects : nullptr);
        const auto result = GateConsole::run_console(encounter, std::cin, std::cout);
        if (trace)
            std::cerr << "Introduce effects: " << effects << '\n';
        return result;
    } catch (const std::exception& failure) {
        std::cerr << "Encounter failure: " << failure.what() << '\n';
        return 1;
    }
}
