#pragma once

#include <assets/definitions/animation.h>
#include <assets/definitions/grid.h>
#include <math/anchor.h>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace CE::Assets {
    struct SpriteDefinition {
        std::string name_space;
        std::string name;
        std::string description;
        std::filesystem::path texture;
        GridDefinition grid;
        math::Pivot pivot;
        std::unordered_map<std::string, ViewDefinition> views;
        std::unordered_map<std::string, CellIndex> orientations;
        std::optional<std::string> animation_profile;
        std::vector<SpriteAnimationDefinition> animations;

        [[nodiscard]] std::string id() const;
    };
}
