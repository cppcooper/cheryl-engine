#pragma once

#include <assets/definitions/animation.h>
#include <assets/definitions/sprite.h>
#include <assets/definitions/tileset.h>
#include <math/anchor.h>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace CE::Assets::Manifests {
    /** Owned CPU definitions. Parsed texture paths already include the graphics root
     * and lexical normalization; do not prepend the document directory again.
     * Loader validates cross-document IDs and image bounds before GPU construction.
     */
    struct Graphics {
        std::filesystem::path source;
        std::string schema;
        std::string version;
        std::string name_space;
        CE::Rendering::Geometry::Pivot default_sprite_pivot;
        CE::Rendering::Geometry::Pivot default_tileset_pivot;
        std::optional<std::filesystem::path> texture;
        std::unordered_map<std::string, CE::Assets::Sprites::Profile> animation_profiles;
        std::vector<CE::Assets::Sprites::Definition> sprites;
        std::vector<CE::Assets::Tilesets::Definition> tilesets;
        std::optional<std::string> shader = {};

        // Owned deduplicated entry paths, sprites then tilesets in first-reference
        // order. The manifest-level default is included only when an entry uses it.
        [[nodiscard]] std::vector<std::filesystem::path> textures() const;
    };
}
