#pragma once

#include <internals/exceptions.h>
#include <miniaudio.h>

#include <filesystem>
#include <new>
#include <string>

namespace CE::Audio::Miniaudio::Detail {
    static_assert(MA_VERSION_MAJOR == 0 && MA_VERSION_MINOR == 11 && MA_VERSION_REVISION >= 25,
        "Cheryl requires miniaudio 0.11.25 or a compatible later 0.11 release");

    inline void check(ma_result result, const char* operation) {
        if (result == MA_OUT_OF_MEMORY)
            throw std::bad_alloc();
        if (result != MA_SUCCESS)
            throw Exceptions::runtime_exception(CE_HERE, std::string(operation) + ": " + ma_result_description(result));
    }

    inline void validate_path(const std::filesystem::path& file) {
        if (file.empty() || file.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos)
            throw Exceptions::invalid_args(CE_HERE, "Audio file path must be nonempty and contain no NUL");
    }
}
