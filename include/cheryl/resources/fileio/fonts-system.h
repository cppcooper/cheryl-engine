#pragma once
#ifndef FONTS_SYSTEM_H
#define FONTS_SYSTEM_H
#include <vector>
#include <filesystem>

namespace CE::Resources {
    extern std::vector<std::filesystem::path> find_system_fonts();
}
#endif //FONTS_SYSTEM_H
