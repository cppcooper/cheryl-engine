#pragma once

#include <cheryl/maps/tilemap.h>
#include <assets/submission/draw2d.h>

#include <vec2.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace CE::Rendering::Submission {
    /** Caller-owned placement, culling and clock values, borrowed only during resolution. */
    struct TilemapView {
        glm::dvec2 origin{};                 ///< Map top-left in world X-right/Y-up units; finite and float-representable.
        CE::Tilemaps::CellSize cell_size;    ///< Positive map-local cell dimensions, independent of source frame dimensions.
        CE::Tilemaps::LocalRect visible;     ///< Map-local X-right/Y-down visible bounds, using visible_cells() computed edges.
        std::chrono::milliseconds elapsed{}; ///< Nonnegative instance time; shared tileset metadata never advances.
    };

    class TilemapBindings;

    /** Resolve one layer into an owned row/column-ordered packet vector, without pass insertion.
     * Stable inputs are borrowed for this call; packets copy matrices and retain resources
     * independently of maps, bindings and asset metadata. Zero IDs emit no packet; missing
     * visible IDs throw bad_request with logical ID, layer and cell coordinates.
     * Requires identity style.model_matrix, style.scale == 1, finite pass matrices,
     * nonnegative elapsed time, and valid visible_cells() inputs. Placement owns the
     * per-cell positive scale and pivot translation; unrepresentable float transforms
     * reject. Ordinary float rounding still applies; arbitrary rotation/overhang is unsupported.
     * Origins, cell dimensions and whole-map far edges must remain finite floats even
     * for an empty visible range. Emitted source-to-cell scales must not round to zero.
     * Invalid/moved-from layers throw bad_request; invalid view/transform inputs throw invalid_args.
     * Validation/allocation failures return no vector and leave inputs unchanged. This is
     * no whole-frame transaction; PassWriter remains responsible for authored_order.
     * No native bind, draw or upload occurs. Native use still needs each resource's live domain.
     */
    [[nodiscard]] CE_API std::vector<DrawPacket2D> resolve_tilemap_layer(
        const CE::Tilemaps::Map& map,
        CE::Tilemaps::LayerIndex layer,
        const TilemapBindings& bindings,
        const TilemapView& view,
        const DrawStyle2D& style,
        const Context2D& context
    );

    /** Setup-owned logical-ID table retaining source handles and original targets.
     * Copies own independent tables sharing the same assets; retention neither deep-copies
     * nor freezes an asset. Bound source metadata must remain unchanged through EVERY
     * alias for the lifetime of ANY binding retaining it, including table copies.
     * No replacement/removal or mutable binding view is exposed. Keep the table/assets
     * and resources stable during each resolution; synchronize externally across owners.
     * This does not define a save format or perform ongoing metadata revalidation.
     */
    class CE_TYPE_API TilemapBindings final {
        struct Binding {
            std::shared_ptr<const CE::Assets::Tilesets::Asset> asset;
            CE::Assets::Grids::CellIndex target{};
        };

        std::unordered_map<std::uint32_t, Binding> bindings_;

        friend std::vector<DrawPacket2D> resolve_tilemap_layer(
            const CE::Tilemaps::Map& map,
            CE::Tilemaps::LayerIndex layer,
            const TilemapBindings& bindings,
            const TilemapView& view,
            const DrawStyle2D& style,
            const Context2D& context
        );

    public:
        CE_API void
        add(CE::Tilemaps::TileId id, std::shared_ptr<const CE::Assets::Tilesets::Asset> asset, CE::Assets::Grids::CellIndex target);
    };
}

/**
 * @fn void CE::Rendering::Submission::TilemapBindings::add(CE::Tilemaps::TileId id, std::shared_ptr<const CE::Assets::Tilesets::Asset> asset, CE::Assets::Grids::CellIndex target)
 * @brief Retain one nonzero logical identity bound to an original atlas target, including target zero.
 * @param id Application-owned nonzero identity; duplicates are rejected.
 * @param asset Retained source handle with nonzero grid/frame, normalized pivot and an image containing the grid. Metadata must stay unchanged through every alias while any binding retains it.
 * @param target Original in-grid target; each resolution applies its animation exactly once at the view's time.
 * @throws CE::Exceptions::invalid_args For zero/duplicate IDs, null assets/images, invalid grid/pivot/animation metadata or targets.
 * All clips require valid targets/frames and positive, representable timelines; unrelated views/autotile rules are not inspected.
 * Resource/material/range compatibility remains resolve_tile()'s responsibility for each visible cell.
 * Validation/allocation failure preserves earlier bindings. No native work or clock mutation occurs.
 */
