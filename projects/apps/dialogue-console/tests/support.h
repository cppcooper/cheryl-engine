#pragma once

#include "../src/files.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>

namespace GateTesting {
    class Directory {
        std::filesystem::path path_;

    public:
        Directory() {
            static unsigned sequence = 0;
            for (int attempt = 0; attempt != 100; ++attempt) {
                const auto candidate = std::filesystem::current_path() / ("gate-fixture-" + std::to_string(++sequence));
                if (std::filesystem::create_directory(candidate)) {
                    path_ = candidate;
                    return;
                }
            }
            throw std::runtime_error("No fresh clone-local fixture directory");
        }
        Directory(const Directory&) = delete;
        Directory& operator=(const Directory&) = delete;
        ~Directory() {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }
        std::string file(std::string_view name) const { return (path_ / name).string(); }
    };

    inline void write_fixture(const std::string& path, std::string_view bytes) {
        std::ofstream output(path, std::ios::binary);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
    }

    inline std::string contents(const std::string& path) {
        std::ifstream input(path, std::ios::binary);
        input.exceptions(std::ios::badbit);
        if (!input)
            throw std::runtime_error("Cannot inspect test artifact: " + path);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
}
