#pragma once

#include <cstdio>
#include <filesystem>
#include <functional>

namespace CE::Logging {
    enum class Level { trace, debug, info, warn, err, critical, off };

    struct FileEventHandlers {
        // Callbacks borrow the filename and FILE* only during the call. Opening
        // callbacks may reject construction; closing failures are contained and
        // reported after delegate locks unwind. Keep captured dependencies alive
        // through final closure. Ordinary recursive logging is suppressed.
        std::function<void(const std::filesystem::path&)> before_open;
        std::function<void(const std::filesystem::path&, std::FILE*)> after_open;
        std::function<void(const std::filesystem::path&, std::FILE*)> before_close;
        std::function<void(const std::filesystem::path&)> after_close;
    };
}
