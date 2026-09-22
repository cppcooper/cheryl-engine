#include <core/resources/fileio/fonts-system.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

namespace CE::Resources {
    namespace {
        namespace fs = std::filesystem;

        std::string lowercase(std::string value) {
            std::ranges::transform(value, value.begin(),
                                   [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        void append_environment_path(std::vector<fs::path>& directories, const char* variable,
                                     const fs::path& suffix = {}) {
            if (const char* value = std::getenv(variable); value && *value) {
                directories.push_back(fs::path(value) / suffix);
            }
        }

        bool is_supported_font(const fs::path& path) {
            const auto extension = lowercase(path.extension().string());
            return extension == ".ttf" || extension == ".otf" || extension == ".ttc" || extension == ".otc";
        }
    }

    std::vector<fs::path> system_font_directories() {
        std::vector<fs::path> directories{"/usr/share/fonts", "/usr/local/share/fonts", "/Library/Fonts",
                                          "/System/Library/Fonts"};
        append_environment_path(directories, "HOME", ".fonts");
        append_environment_path(directories, "HOME", ".local/share/fonts");
        append_environment_path(directories, "HOME", "Library/Fonts");
        append_environment_path(directories, "XDG_DATA_HOME", "fonts");
        append_environment_path(directories, "WINDIR", "Fonts");
        append_environment_path(directories, "LOCALAPPDATA", "Microsoft/Windows/Fonts");
        return directories;
    }

    std::vector<fs::path> find_system_fonts() {
        return find_system_fonts(system_font_directories());
    }

    std::vector<fs::path> find_system_fonts(const std::vector<fs::path>& directories) {
        std::vector<fs::path> fonts;
        for (const auto& directory : directories) {
            std::error_code error;
            if (!fs::is_directory(directory, error) || error)
                continue;

            fs::recursive_directory_iterator entries(directory, fs::directory_options::skip_permission_denied, error);
            const fs::recursive_directory_iterator end;
            while (!error && entries != end) {
                const auto& entry = *entries;
                if (entry.is_regular_file(error) && !error && is_supported_font(entry.path())) {
                    fonts.push_back(entry.path().lexically_normal());
                }
                entries.increment(error);
                if (error)
                    error.clear();
            }
        }
        std::ranges::sort(fonts);
        fonts.erase(std::unique(fonts.begin(), fonts.end()), fonts.end());
        return fonts;
    }

    std::optional<fs::path> select_default_system_font(const std::vector<fs::path>& fonts) {
        if (fonts.empty())
            return std::nullopt;
        constexpr std::string_view preferred_names[]{"arial.ttf",
                                                     "helvetica.ttc",
                                                     "dejavusans.ttf",
                                                     "liberationsans-regular.ttf",
                                                     "nimbussans-regular.otf",
                                                     "notosans-regular.ttf",
                                                     "freesans.ttf",
                                                     "segoeui.ttf",
                                                     "roboto-regular.ttf",
                                                     "calibri.ttf",
                                                     "consola.ttf",
                                                     "proggyvector regular.ttf"};
        for (const auto preferred : preferred_names) {
            const auto match = std::ranges::find_if(
                fonts, [preferred](const fs::path& font) { return lowercase(font.filename().string()) == preferred; });
            if (match != fonts.end())
                return *match;
        }
        return *std::ranges::min_element(fonts);
    }
}
