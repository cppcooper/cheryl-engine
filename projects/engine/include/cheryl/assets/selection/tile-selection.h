#pragma once

#include <cheryl/export.h>

#include <assets/definitions/tileset.h>

#include <cstdint>
#include <functional>
#include <variant>

namespace CE::Assets::Tilesets::Selection {
    /** Kind of application-owned site sampled relative to the selected tile. */
    enum class SiteKind { Cell, ///< Neighboring cell for bitmask connectivity.,
        Edge, ///< Boundary shared with a cardinal neighbor.,
        Corner ///< Vertex shared with diagonal/corner neighbors.
    };

    // Relative to the selected tile. Wang labels belong to shared edges/vertices;
    // bitmask labels belong to neighboring cells. The caller owns world coordinates.
/** Site kind/direction relative to the tile; application sampler supplies actual world coordinates. */
    struct Site {
        SiteKind kind{}; ///< Cell, shared edge or shared corner to inspect.
        Direction direction{}; ///< Canonical direction from the selected tile.

        [[nodiscard]] bool operator==(const Site&) const = default;
    };

    /** Whether a sampled terrain label can be used directly. */
    enum class SampleState { Known, ///< Use terrain, including zero for empty.,
        Outside, ///< Site lies outside application-defined world bounds; apply outside fallback.,
        Unknown ///< Site is unavailable/unloaded or its label unknown; apply unknown fallback.
    };

/** One copied neighborhood observation; construction does not validate enum values. */
    struct Sample {
        SampleState state{SampleState::Unknown}; ///< Known, outside-world or unavailable observation.
        std::uint32_t terrain{}; ///< Known terrain label; zero means empty and other sample states ignore this field.
    };

    // Invoked synchronously for each required site once, in canonical direction order.
    // Keep the sampled world stable for the call. The selector never retains this callback.
    /** Synchronous non-retained callback, once per required site in canonical direction order. Keep world data stable for the call; exceptions propagate. */
    using Sampler = std::function<Sample(Site)>;

    /** Substitution policy for unavailable neighborhood labels. */
    enum class Fallback { Empty, ///< Substitute terrain zero.,
        Center, ///< Substitute Options::terrain.,
        Unresolved ///< Return IncompleteNeighborhood when this site is required.
    };
    /** Additional bitmask diagonal gating; does not change Wang site sampling. */
    enum class DiagonalConnectivity { Independent, ///< Allow matching diagonal cells without adjacent cardinal connectivity.,
        RequireCardinals ///< Require both adjacent cardinal cells to connect before setting a diagonal bit.
    };

/** CPU-only sampling/fallback and deterministic variant selection; owns no world or animation clock. */
    struct Options {
        std::uint32_t terrain{}; ///< Center terrain label; zero never connects in a bitmask.
        std::uint64_t seed{}; ///< Caller-owned location/variant seed; fixed metadata+samples+seed give deterministic selection, independent of animation time.
        Fallback outside{Fallback::Empty}; ///< Fallback for application world-boundary observations.
        Fallback unknown{Fallback::Unresolved}; ///< Fallback for unavailable samples and undeclared Wang labels.
        DiagonalConnectivity diagonals{DiagonalConnectivity::RequireCardinals}; ///< Whether bitmask diagonal connectivity also requires its adjacent cardinal cells.
    };

    /** Ordinary unresolved selection result, distinct from invalid metadata exceptions. */
    enum class Failure { MissingRule, ///< No exact signature/mask candidate exists.,
        IncompleteNeighborhood ///< At least one required site remains unresolved after fallback.
    };
    /** Selected original row-major cell or ordinary missing/incomplete failure; no animation substitution has occurred. */
    using Result = std::variant<CE::Assets::Grids::CellIndex, Failure>;

    // CPU-only lookup: Wang candidates retain definition order and use finite positive
    // weights; bit_order owns bit positions. Unknown Wang IDs use the unknown policy.
    // Diagonal gating also samples adjacent cardinal cells absent from bit_order.
    // Missing/empty candidate lists return MissingRule; unresolved required samples
    // return IncompleteNeighborhood. Invalid options/rule metadata or an empty sampler
    // throw invalid_args; sampler exceptions propagate. No grid or animation lookup here.
    [[nodiscard]] CE_API Result select(const WangRule& rule, const Sampler& sampler, const Options& options);
    [[nodiscard]] CE_API Result select(const BitmaskRule& rule, const Sampler& sampler, const Options& options);
    [[nodiscard]] CE_API Result select(const Rule& rule, const Sampler& sampler, const Options& options);
}

/**
 * @fn bool CE::Assets::Tilesets::Selection::Site::operator==(const Site&) const
 * @brief Compare both site kind and direction, independent of application world position.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#tile-selection)
 */

/**
 * @fn CE::Assets::Tilesets::Selection::Result CE::Assets::Tilesets::Selection::select(const WangRule& rule, const Sampler& sampler, const Options& options)
 * @brief Match shared edge/corner labels and choose a finite-positive weighted variant in definition order.
 * @param rule Stable CPU rule metadata; cells are not checked against an image/grid here.
 * @param sampler Nonempty synchronous callback, never retained; keep sampled world data stable.
 * @param options Valid fallback/diagonal choices and deterministic location seed.
 * @return Selected original cell, MissingRule for an absent/empty candidate set, or IncompleteNeighborhood for unresolved required sites.
 * @throws CE::Exceptions::invalid_args For invalid options, sampler or relevant rule metadata. Sampler/allocation exceptions propagate.
 * Unknown Wang terrain IDs use the unknown policy. No world storage, native resource, grid validation or animation lookup occurs; apply an owning tileset's cell_at() separately when needed.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#tile-selection)
 */

/**
 * @fn CE::Assets::Tilesets::Selection::Result CE::Assets::Tilesets::Selection::select(const BitmaskRule& rule, const Sampler& sampler, const Options& options)
 * @brief Build a neighboring-cell connectivity mask using bit_order; diagonal gating may sample extra cardinal cells.
 * @param rule Stable CPU rule metadata; cells are not checked against an image/grid here.
 * @param sampler Nonempty synchronous callback, never retained; keep sampled world data stable.
 * @param options Valid fallback/diagonal choices and deterministic location seed.
 * @return Selected original cell, MissingRule for an absent/empty candidate set, or IncompleteNeighborhood for unresolved required sites.
 * @throws CE::Exceptions::invalid_args For invalid options, sampler or relevant rule metadata. Sampler/allocation exceptions propagate.
 * Unknown Wang terrain IDs use the unknown policy. No world storage, native resource, grid validation or animation lookup occurs; apply an owning tileset's cell_at() separately when needed.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#tile-selection)
 */

/**
 * @fn CE::Assets::Tilesets::Selection::Result CE::Assets::Tilesets::Selection::select(const Rule& rule, const Sampler& sampler, const Options& options)
 * @brief Visit the stored Wang or bitmask rule without changing its semantics.
 * @param rule Stable CPU rule metadata; cells are not checked against an image/grid here.
 * @param sampler Nonempty synchronous callback, never retained; keep sampled world data stable.
 * @param options Valid fallback/diagonal choices and deterministic location seed.
 * @return Selected original cell, MissingRule for an absent/empty candidate set, or IncompleteNeighborhood for unresolved required sites.
 * @throws CE::Exceptions::invalid_args For invalid options, sampler or relevant rule metadata. Sampler/allocation exceptions propagate.
 * Unknown Wang terrain IDs use the unknown policy. No world storage, native resource, grid validation or animation lookup occurs; apply an owning tileset's cell_at() separately when needed.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#tile-selection)
 */
