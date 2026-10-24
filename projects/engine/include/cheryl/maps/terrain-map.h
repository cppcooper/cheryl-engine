#pragma once

#include <cheryl/export.h>
#include <assets/selection/tile-selection.h>
#include <maps/tilemap.h>

#include <optional>
#include <span>
#include <vector>

namespace CE::Tilemaps {
    /** Semantic equality label, independent of visual TileId or atlas targets. */
    struct TerrainLabel {
        std::uint32_t value{}; ///< Zero is empty; every other uint32 value is an opaque terrain identity.
        bool operator==(const TerrainLabel&) const = default;
    };

    /** One semantic assignment in a borrowed batch; duplicate points use the last label. */
    struct TerrainEdit {
        Point point;        ///< Signed in-bounds X-right/Y-down cell, validated even when later overwritten.
        TerrainLabel label; ///< Any uint32 label is valid; zero clears, with no application meaning inferred.
        bool operator==(const TerrainEdit&) const = default;
    };

    /** Hard per-call admission limits, not reservations or CPU-time measurements. */
    struct TerrainBatchLimits {
        std::size_t requests{};       ///< Maximum input records, including duplicates, no-ops and cancellations.
        std::size_t affected_cells{}; ///< Maximum unique returned/recomputed cells, including empty neighbors.
        std::size_t work{};           ///< Request/candidate/resolution/sample units; see TerrainMap::edit_batch.
    };

    /** Own a finite cardinal terrain layer and its derived visual Map.
     * Link Cheryl::Engine. Inputs are copied; no assets, samplers or application
     * terrain meanings are retained. The owner is neither copyable nor movable.
     * Serialize all access, edits and packet preparation; this is not a World,
     * frame transaction or synchronized store. Prepared packets retain their own
     * resources and matrices independently of this CPU owner.
     */
    class CE_TYPE_API TerrainMap final {
        struct Change {
            CE::Tilemaps::Point point;
            TerrainLabel label;
        };

        CE::Tilemaps::Map tiles_;
        CE::Tilemaps::LayerIndex layer_;
        std::vector<TerrainLabel> labels_;
        CE::Assets::Tilesets::BitmaskRule rule_;
        std::vector<CE::Tilemaps::TileId> target_to_id_;

        [[nodiscard]] bool contains(CE::Tilemaps::Point point) const;
        [[nodiscard]] std::size_t index(CE::Tilemaps::Point point) const;
        [[nodiscard]] CE::Tilemaps::TileId resolve(CE::Tilemaps::Point point, std::optional<Change> change) const;

    public:
        CE_API TerrainMap(
            const CE::Tilemaps::Map& base,
            CE::Tilemaps::LayerIndex layer,
            std::span<const TerrainLabel> labels,
            const CE::Assets::Tilesets::Rule& rule,
            std::span<const CE::Tilemaps::TileId> target_to_id
        );
        TerrainMap(const TerrainMap&) = delete;
        TerrainMap& operator=(const TerrainMap&) = delete;
        CE_API ~TerrainMap();

        [[nodiscard]] CE_API TerrainLabel label(CE::Tilemaps::Point point) const;
        [[nodiscard]] CE_API const CE::Tilemaps::Map& tiles() const;
        [[nodiscard]] CE_API std::vector<CE::Tilemaps::Point> edit(CE::Tilemaps::Point point, TerrainLabel label);
        [[nodiscard]] CE_API std::vector<Point> edit_batch(std::span<const TerrainEdit> requests, TerrainBatchLimits limits);
    };
}

/**
 * @fn CE::Tilemaps::TerrainMap::TerrainMap(const CE::Tilemaps::Map& base, CE::Tilemaps::LayerIndex layer, std::span<const CE::Tilemaps::TerrainLabel> labels, const CE::Assets::Tilesets::Rule& rule, std::span<const CE::Tilemaps::TileId> target_to_id)
 * @brief Copy inputs and derive one complete visual layer before returning.
 * @param base Nonempty, live finite Map copied in full; other layers are preserved.
 * @param layer Layer to derive, within base.layer_count().
 * @param labels Exactly columns*rows row-major labels; X-right/Y-down coordinates.
 * @param rule FourNeighbor BitmaskRule with 1..4 unique cardinal directions.
 * bit_order position supplies its bit (N/E/S/W means 1/2/4/8); subsets and
 * reordered cardinal directions are supported. Case keys must fit that order.
 * @param target_to_id Dense target-index table copied into the owner. Its length
 * defines the target domain; zero entries are unmapped. Target zero is valid.
 * Only equal nonzero labels connect. Outside samples use Empty fallback; empty
 * centers derive TileId{0} without lookup. Nonempty centers require an exact case
 * and a mapped nonzero logical ID, which is not validated against atlas bindings.
 * Missing cases/holes may remain unused, including entirely empty rules/tables
 * for an empty semantic grid. No diagonals, Wang, Unknown, wrapping or clamping.
 * @throws CE::Exceptions::invalid_args Invalid shape/count/storage product or rule metadata.
 * @throws CE::Exceptions::bad_request Invalid layer, or a used missing case/target.
 * Allocation failures propagate. Construction never mutates caller inputs.
 * Inputs need only remain stable during the call; the owner copies all data.
 */

/**
 * @fn CE::Tilemaps::TerrainMap::~TerrainMap()
 * @brief Release owned CPU data through Engine; no native context is required.
 * Borrowed tiles() references expire. Already prepared packets remain independent.
 */

/**
 * @fn CE::Tilemaps::TerrainLabel CE::Tilemaps::TerrainMap::label(CE::Tilemaps::Point point) const
 * @return Owned semantic label at an in-bounds signed cell coordinate.
 * @throws CE::Exceptions::bad_request Point is outside the finite map; no mutation.
 */

/**
 * @fn const CE::Tilemaps::Map& CE::Tilemaps::TerrainMap::tiles() const
 * @return Borrowed const Map, valid until owner destruction; edits change its selected layer.
 * Exclude concurrent editing while reading it or preparing packets. No allocation.
 */

/**
 * @fn std::vector<CE::Tilemaps::Point> CE::Tilemaps::TerrainMap::edit(CE::Tilemaps::Point point, CE::Tilemaps::TerrainLabel label)
 * @brief Derive all affected results from semantic neighbors before publishing one edit.
 * @param point In-bounds signed coordinate, checked before neighbor arithmetic.
 * @param label New semantic label; zero clears, an unchanged label is a no-op.
 * @return Owned unique affected cells: center, then in-bounds N/E/S/W, including
 * empty neighbors; no diagonals. Unchanged labels return an empty vector.
 * @throws CE::Exceptions::bad_request Invalid point or a needed missing case/target.
 * All results and return storage precede publication. Any refusal or allocation
 * failure preserves every label and visual cell; other layers never change.
 * Work is bounded to five cells, each sampling at most four neighbors. No call
 * arguments or sampler references survive. Serialize access as for tiles().
 */

/**
 * @fn std::vector<CE::Tilemaps::Point> CE::Tilemaps::TerrainMap::edit_batch(std::span<const CE::Tilemaps::TerrainEdit> requests, CE::Tilemaps::TerrainBatchLimits limits)
 * @brief Publish a bounded stroke atomically, deriving from its final semantic state.
 * @param requests Stable for this call only. Validate every point first, then
 * coalesce duplicates last-write-wins. All uint32 labels are valid. Centers whose
 * final label equals the original are excluded, even after intermediate changes.
 * @param limits Hard maxima. One work unit admits each request, visits each net
 * changed center and each of its four cardinal candidates (including exterior),
 * resolves each unique affected cell (even empty), and invokes each selector
 * sample (even exterior). Resolution includes selection for nonempty centers.
 * Debiting remaining work cannot overflow; no allocation is sized from a cap.
 * @return Owned unique center/cardinal union, ordered by first request occurrence
 * of net changed centers, visiting center/N/E/S/W and retaining first occurrence.
 * Existing edit() keeps its original order. Empty input costs nothing; no-op or
 * cancelling input costs request units only and returns empty. Semantic changes
 * still invalidate when their visual IDs are equal. No diagonals are added.
 * @throws CE::Exceptions::bad_request Any invalid point, exhausted cap or needed
 * missing mask/target. No request, even an overwritten one, hides an invalid point.
 * Complete owned proposed labels, visual results and output exist before either
 * domain publishes. Any refusal/allocation failure preserves the whole owner.
 * Scratch is O(requests + affected_cells), with ordered lookups bounded by those
 * limits; work units bound visits/selection/sampling, not allocator/comparison
 * instructions. No full-grid copy is made. Inputs/callbacks are not retained.
 * Serialize access as for tiles(); no concurrent reads or whole-frame rollback.
 */
