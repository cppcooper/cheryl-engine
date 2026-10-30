#pragma once

#include <cheryl/export.h>

#include <assets/definitions/animation.h>
#include <assets/definitions/grid.h>
#include <math/anchor.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace CE::Assets::Tilesets {
    /** Canonical clockwise sites relative to a tile; the application maps these to world coordinates. */
    enum class Direction { North, ///< North cardinal site.,
        NorthEast, ///< Northeast diagonal/corner site.,
        East, ///< East cardinal site.,
        SouthEast, ///< Southeast diagonal/corner site.,
        South, ///< South cardinal site.,
        SouthWest, ///< Southwest diagonal/corner site.,
        West, ///< West cardinal site.,
        NorthWest ///< Northwest diagonal/corner site.
    };

    /** Which shared boundary sites determine a Wang signature. */
    enum class WangType { Corner, ///< Use four shared corner labels in odd signature slots.,
        Edge ///< Use four shared edge labels in even signature slots.
    };

/** Owned terrain-catalogue metadata; zero labels denote empty rather than an additional terrain. */
    struct Terrain {
        std::uint32_t id{}; ///< Declared terrain label; zero is reserved for empty.
        std::string name; ///< Human-readable terrain name.
        std::optional<std::string> color; ///< Optional authored color metadata, not a render material.
        double probability{1.0}; ///< Authored positive probability metadata; select() weights variants using WangTile::weight.
        std::optional<CE::Assets::Grids::CellIndex> representative; ///< Optional illustrative grid cell for this terrain, not an automatic fallback.
    };

    /** Eight terrain labels in N, NE, E, SE, S, SW, W, NW order. Inactive edge/corner slots are zero. */
    using WangSignature = std::array<std::uint32_t, 8>;

/** Stateless lookup hash for all eight signature labels; not a serialized stable identity. */
    struct WangSignatureHash {
        [[nodiscard]] CE_API std::size_t operator()(const WangSignature& signature) const noexcept;
    };

/** Candidate grid cell and finite positive relative selection weight for an exact signature. */
    struct WangTile {
        CE::Assets::Grids::CellIndex cell{}; ///< Candidate row-major cell in the owning tileset grid.
        WangSignature wang{}; ///< Exact canonical signature to which this tile belongs.
        double weight{1.0}; ///< Finite positive relative variant weight; no external/shared RNG is used.
    };

    /** Parsed terrain signatures and candidate tile indices in definition order.
     * CPU selection samples shared edge/corner labels and chooses a weighted variant.
     */
    struct WangRule {
        std::string description; ///< Owned human-readable rule description.
        WangType type{}; ///< Whether to sample shared edges or shared corners.
        std::array<Direction, 8> slot_order{}; ///< Must equal canonical N, NE, E, SE, S, SW, W, NW order.
        std::vector<Terrain> terrains; ///< Declared nonempty terrain labels; sampled zero remains empty.
        std::vector<WangTile> tiles; ///< Candidate tiles in authored definition order.
        std::unordered_map<WangSignature, std::vector<std::size_t>, WangSignatureHash> variants; ///< Signature to strictly increasing indices into tiles, preserving deterministic definition order.
    };

    /** Allowed neighborhood directions for cell connectivity. */
    enum class BitmaskType { FourNeighbor, ///< Unique cardinal directions only, with at most four declared bits.,
        EightNeighbor ///< Cardinal/diagonal directions, with at most eight declared bits.
    };

/** Owned bit-position order and mask-to-cell mapping; aggregate assignment does not validate metadata. */
    struct BitmaskRule {
        std::string description; ///< Owned human-readable rule description.
        BitmaskType type{}; ///< Four-cardinal or eight-neighbor direction family.
        std::vector<Direction> bit_order; ///< Unique compatible directions; vector index is the mask bit position.
        std::unordered_map<std::uint32_t, CE::Assets::Grids::CellIndex> cases; ///< Exact generated mask to row-major cell; an absent case yields MissingRule.
    };

    /** Either exact shared-site Wang matching or neighboring-cell bitmask matching. */
    using Rule = std::variant<WangRule, BitmaskRule>;

/** Owned tileset/grid metadata with independent named views, timelines and CPU autotile rules. */
    struct Definition {
        std::string name_space; ///< Namespace qualifying id().
        std::string name; ///< Local asset name.
        std::string description; ///< Human-readable metadata, not a gameplay rule.
        std::filesystem::path texture; ///< Resolved image source path; parsed values already include graphics root.
        CE::Assets::Grids::Definition grid; ///< Source pixel cells, spacing and row-major addressing.
        CE::Rendering::Geometry::Pivot pivot; ///< Finite normalized top-left-origin pivot used for Y-up cell geometry.
        std::unordered_map<std::string, CE::Assets::Grids::View> views; ///< Named rectangular regions in cell coordinates.
        std::unordered_map<std::string, CE::Assets::Grids::CellIndex> orientations; ///< Named static row-major cells, independent of time.
        std::unordered_map<std::string, Clip> animations; ///< Named owned clips; each original target maps to at most one clip in a constructed Asset.
        std::unordered_map<std::string, Rule> autotiles; ///< Named Wang/bitmask definitions sampled through application-owned world callbacks.
        std::optional<std::string> shader = {}; ///< Optional qualified material override; absence selects loader-owned ordinary image defaults.

        [[nodiscard]] CE_API std::string id() const;
    };
}

/**
 * @fn std::size_t CE::Assets::Tilesets::WangSignatureHash::operator()(const WangSignature& signature) const noexcept
 * @brief Hash all eight ordered terrain labels for an in-process unordered lookup.
 * @param signature Exact canonical label array.
 * @return size_t hash; equality still compares full signatures. Do not persist the hash as a terrain/signature identifier.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#tile-selection)
 */

/**
 * @fn std::string CE::Assets::Tilesets::Definition::id() const
 * @brief Form the exact namespace:name lookup ID without validating or registering it.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#tile-selection)
 */
