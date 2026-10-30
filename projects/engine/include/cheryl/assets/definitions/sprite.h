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
/** Owned sprite metadata; parsing/loading validates it, aggregate assignment does not.
     * @see [Sprite definitions](cheryl-guide:assets/asset-manifests)
     */
    struct Definition {
        std::string name_space; ///< Namespace used to qualify this definition's ID.
        std::string name; ///< Local asset name, combined with namespace by id().
        std::string description; ///< Human-readable description, not a behavior contract.
        std::filesystem::path texture; ///< Resolved texture source path; parsed values already include the enclosing graphics root.
        CE::Assets::Grids::Definition grid; ///< Row-major source cells, spacing and pixel origin.
        CE::Rendering::Geometry::Pivot pivot; ///< Finite normalized top-left-origin pivot used for local Y-up geometry.
        std::unordered_map<std::string, CE::Assets::Grids::View> views; ///< Named cell-coordinate regions borrowing no separate image.
        std::unordered_map<std::string, CE::Assets::Grids::CellIndex> orientations; ///< Named static cell selections, independent of animation clocks.
        std::optional<std::string> animation_profile; ///< Optional profile source name; concrete animations hold expanded playback clips.
        std::vector<Clip> animations; ///< Concrete clips in definition order, with exact name/facing identity.
        std::optional<std::string> shader = {}; ///< Optional qualified material override; absence lets the loader use its ordinary image-material default.

        [[nodiscard]] CE_API std::string id() const;
    };
}

/**
 * @fn std::string CE::Assets::Sprites::Definition::id() const
 * @brief Form an owned namespace:name lookup ID.
 * @return Exact concatenation with a colon; performs no naming validation or registration.
 * @see [Detailed guide](cheryl-guide:assets/asset-manifests)
 */
