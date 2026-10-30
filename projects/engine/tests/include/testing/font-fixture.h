#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace CE::Testing {
    inline const auto font_fixture_source = std::filesystem::path(CHERYL_SOURCE_DIR) / "assets/fonts/DejaVuSans.ttf";

    struct FontFixture {
        std::filesystem::path path;
        inline static std::atomic<unsigned> next{};

        FontFixture() {
            const auto root = std::filesystem::temp_directory_path();
            for (unsigned i = 0; i < 64; ++i) {
                path = root / ("cheryl-font-service-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                               std::to_string(next++));
                if (std::filesystem::create_directory(path))
                    return;
            }
            throw std::runtime_error("Cannot create font service fixture directory");
        }
        ~FontFixture() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };

    inline std::vector<unsigned char> font_bytes() {
        std::ifstream input(font_fixture_source, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    inline std::uint32_t read(const std::vector<unsigned char>& bytes, const std::size_t offset, const unsigned width) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value = (value << 8) | bytes.at(offset + i);
        return value;
    }

    inline void write(std::vector<unsigned char>& bytes, const std::size_t offset, const unsigned width, std::uint32_t value) {
        for (unsigned i = width; i != 0; --i) {
            bytes.at(offset + i - 1) = static_cast<unsigned char>(value & 255);
            value >>= 8;
        }
    }

    inline void save(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!output)
            throw std::runtime_error("Cannot write font service fixture");
    }

    inline void styled_file(const std::filesystem::path& path, const std::uint16_t weight, const bool italic) {
        auto bytes = font_bytes();
        const auto tables = read(bytes, 4, 2);
        for (std::size_t i = 0; i < tables; ++i) {
            const auto entry = 12 + i * 16;
            const auto tag = read(bytes, entry, 4);
            const auto offset = read(bytes, entry + 8, 4);
            if (tag == 0x4f532f32) { // OS/2 weight and style selection.
                write(bytes, offset + 4, 2, weight);
                write(bytes, offset + 62, 2, (italic ? 1u : 0u) | (weight >= 700 ? 32u : 0u));
            }
            if (tag == 0x68656164) // head.macStyle.
                write(bytes, offset + 44, 2, (italic ? 2u : 0u) | (weight >= 700 ? 1u : 0u));
        }
        save(path, bytes);
    }

    inline void collection_file(const std::filesystem::path& path) {
        const auto original = font_bytes();
        std::vector<unsigned char> bytes(20 + original.size() * 2);
        write(bytes, 0, 4, 0x74746366); // ttcf, version 1, two faces.
        write(bytes, 4, 4, 0x00010000);
        write(bytes, 8, 4, 2);
        for (unsigned face = 0; face < 2; ++face) {
            const auto base = 20 + face * original.size();
            write(bytes, 12 + face * 4, 4, static_cast<std::uint32_t>(base));
            std::copy(original.begin(), original.end(), bytes.begin() + static_cast<std::ptrdiff_t>(base));
            const auto tables = read(original, 4, 2);
            for (std::size_t i = 0; i < tables; ++i) {
                const auto entry = 12 + i * 16 + 8;
                write(bytes, base + entry, 4, read(original, entry, 4) + static_cast<std::uint32_t>(base));
                if (face == 1) {
                    const auto table = base + read(original, entry, 4);
                    const auto tag = read(original, entry - 8, 4);
                    if (tag == 0x68686561) // hhea.ascender makes face-index loss observable in metrics.
                        write(bytes, table + 4, 2, 1500);
                    if (tag == 0x4f532f32) {
                        write(bytes, table + 4, 2, 700);
                        write(bytes, table + 62, 2, 32);
                    }
                    if (tag == 0x68656164)
                        write(bytes, table + 44, 2, 1);
                }
            }
        }
        save(path, bytes);
    }
}
