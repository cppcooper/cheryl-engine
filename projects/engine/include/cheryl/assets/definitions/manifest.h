#pragma once

#include <cheryl/export.h>

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
        std::filesystem::path source; ///< Manifest source used for diagnostics and provenance.
        std::string schema; ///< Declared schema URI metadata; runtime does not fetch it.
        std::string version; ///< Declared graphics format version validated by the parser.
        std::string name_space; ///< Namespace inherited by local sprite/tileset names.
        CE::Rendering::Geometry::Pivot default_sprite_pivot; ///< Default normalized pivot copied into parsed sprite definitions.
        CE::Rendering::Geometry::Pivot default_tileset_pivot; ///< Default normalized pivot copied into parsed tileset definitions.
        std::optional<std::filesystem::path> texture; ///< Optional document texture default; textures() returns entry references rather than this unused default.
        std::unordered_map<std::string, CE::Assets::Sprites::Profile> animation_profiles; ///< Owned expansion recipes used while parsing concrete sprite animations.
        std::vector<CE::Assets::Sprites::Definition> sprites; ///< Owned sprite definitions in document order.
        std::vector<CE::Assets::Tilesets::Definition> tilesets; ///< Owned tileset definitions in document order.
        std::optional<std::string> shader = {}; ///< Optional document material selection inherited by entries without their own selection.

        // Owned deduplicated entry paths, sprites then tilesets in first-reference
        // order. The manifest-level default is included only when an entry uses it.
        [[nodiscard]] CE_API std::vector<std::filesystem::path> textures() const;
    };
}

/**
 * @fn std::vector<std::filesystem::path> CE::Assets::Manifests::Graphics::textures() const
 * @brief Collect owned distinct entry texture paths in first-reference order.
 * @return Sprite textures first, then tileset textures, with repeated paths removed by path equality.
 * The manifest-level texture default appears only if an entry references it. Paths are not opened, canonicalized again or resolved relative to the document here.
 * @see [Detailed guide](cheryl-guide:assets/asset-loading)
 */
