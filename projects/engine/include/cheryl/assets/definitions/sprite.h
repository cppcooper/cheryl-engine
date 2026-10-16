#pragma once

#include <cheryl/export.h>

#include <assets/definitions/animation.h>
#include <assets/definitions/grid.h>
#include <math/anchor.h>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace CE::Assets::Sprites {
    struct Definition {
        std::string name_space;
        std::string name;
        std::string description;
        std::filesystem::path texture;
        CE::Assets::Grids::Definition grid;
        CE::Rendering::Geometry::Pivot pivot;
        std::unordered_map<std::string, CE::Assets::Grids::View> views;
        std::unordered_map<std::string, CE::Assets::Grids::CellIndex> orientations;
        std::optional<std::string> animation_profile;
        std::vector<Clip> animations;
        std::optional<std::string> shader = {};

        [[nodiscard]] CE_API std::string id() const;
    };
}
