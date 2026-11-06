#include <cheryl/core/resources/fileio/chunk-file.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace Chunks = CE::Files::Chunks;
int main() {
    const auto path = std::filesystem::temp_directory_path() /
                      ("cheryl-chunks-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    // Exclusive directory creation avoids overwriting another process's file.
    if (!std::filesystem::create_directory(path))
        throw std::runtime_error("temporary directory unavailable");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } cleanup{path};
    try {
        const Chunks::FourCC root{'R', 'O', 'O', 'T'}, test{'T', 'E', 'S', 'T'};
        const Chunks::Bytes payload{std::byte{0}, std::byte{0x41}, std::byte{0}};
        const std::vector values{Chunks::Chunk::container(root, {Chunks::Chunk::leaf(test, payload)})};
        Chunks::write_file(path / "example.chnk", values);
        const auto document = Chunks::read_file(path / "example.chnk");
        if (document.chunks().size() != 1)
            throw std::runtime_error("root count mismatch");
        for (auto group : document.chunks()) {
            if (group.category() != Chunks::Category::container || group.type() != root || group.children().size() != 1)
                throw std::runtime_error("group mismatch");
            for (auto leaf : group.children()) {
                if (leaf.category() != Chunks::Category::data || leaf.type() != test || !std::ranges::equal(leaf.payload(), payload))
                    throw std::runtime_error("payload mismatch");
            }
        }
        std::cout << "Reloaded 51 bytes: CONT/ROOT -> DATA/TEST = 00 41 00\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
