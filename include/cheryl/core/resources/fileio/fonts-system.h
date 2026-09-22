#pragma once
#include <filesystem>
#include <optional>
#include <vector>

namespace CE::Resources {
    [[nodiscard]] std::vector<std::filesystem::path> system_font_directories();
    [[nodiscard]] std::vector<std::filesystem::path> find_system_fonts();
    [[nodiscard]] std::vector<std::filesystem::path>
    find_system_fonts(const std::vector<std::filesystem::path>& directories);
    [[nodiscard]] std::optional<std::filesystem::path>
    select_default_system_font(const std::vector<std::filesystem::path>& fonts);
}
