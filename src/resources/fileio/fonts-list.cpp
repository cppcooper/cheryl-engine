#include <iostream>
#include <filesystem>
#include <vector>

namespace CE::Resources {
    std::vector<std::filesystem::path> find_system_fonts() {
        std::vector<std::filesystem::path> font_paths;

        // Common font directories
        std::vector<std::filesystem::path> font_dirs = {
            "/usr/share/fonts",            // Linux
            "/usr/local/share/fonts",      // Linux
            std::filesystem::path(std::getenv("HOME")) / ".fonts",  // Linux and macOS user fonts
            "/Library/Fonts",              // macOS
            "/System/Library/Fonts",       // macOS
            "C:\\Windows\\Fonts"           // Windows
        };

        // Iterate through directories and search for .ttf or .otf files
        for (const auto& dir : font_dirs) {
            if (exists(dir) && is_directory(dir)) {
                std::filesystem::recursive_directory_iterator recursive_iter(dir);
                // recursively iterate directory's content
                for (auto& entry : recursive_iter) {
                    if (entry.is_regular_file()) {
                        auto ext = entry.path().extension();
                        if (ext == ".ttf" || ext == ".otf") {
                            font_paths.push_back(entry.path());
                        }
                    }
                }
            }
        }

        return font_paths;
    }
}
