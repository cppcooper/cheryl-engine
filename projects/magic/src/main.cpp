#include <magic/compiler.h>

#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {
    Magic::Catalog fixture_catalog() {
        Magic::Catalog catalog;
        catalog.operations = {
            {Magic::Primitive::Heal, "game.heal"}, {Magic::Primitive::Fire, "game.fire"}, {Magic::Primitive::Slow, "game.slow"}
        };
        return catalog;
    }
}

int main(int argc, char** argv) {
    // A deliberately tiny inspect/compose caller: edit text, compile, adopt only on success.
    if (argc > 2 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "Usage: magic_inspect [VERSE_FILE]\n"
                     "Without a file, reads an editable verse from stdin (at most 8192 bytes).\n"
                     "Common@1 is controlled syntax. Mend/Cinder words are invented, not Elvish.\n"
                     "Prints exact Common and owned plan inspection; executes no game effects.\n";
        return argc > 2 ? 2 : 0;
    }
    std::ifstream file;
    std::istream* input = &std::cin;
    if (argc == 2) {
        file.open(argv[1], std::ios::binary);
        if (!file) {
            std::cerr << "Cannot open verse file.\n";
            return 2;
        }
        input = &file;
    }
    std::string source;
    source.reserve(8192);
    char byte;
    while (input->get(byte)) {
        if (source.size() == 8192) {
            std::cerr << "SourceLimit at byte 8192: verse exceeds 8192 bytes.\n";
            return 1;
        }
        source.push_back(byte);
    }
    if (input->bad()) {
        std::cerr << "Verse input read failed.\n";
        return 2;
    }
    const auto result = Magic::compile(source, fixture_catalog());
    if (!result) {
        const auto& error = result.error();
        std::cerr << Magic::code_name(error.code) << " at byte " << error.span.offset << '+' << error.span.length << " [" << error.excerpt
                  << "]: " << error.message << '\n';
        return 1;
    }
    const auto& plan = **result;
    std::cout << plan.inspection();
    std::cout << "Budget: " << plan.metrics().tokens << " tokens, depth " << plan.metrics().nesting << ", " << plan.metrics().work
              << " work units, " << plan.metrics().retained_bytes << " retained logical bytes.\n";
    return 0;
}
