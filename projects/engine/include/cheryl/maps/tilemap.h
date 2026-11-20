#pragma once

#include <cheryl/export.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace CE::Tilemaps {
    /** Application-owned logical identity; zero is empty, independent of any atlas or palette. */
    struct TileId {
        std::uint32_t value{}; ///< Logical value; every uint32_t value is accepted by Map.

        [[nodiscard]] bool operator==(const TileId&) const = default;
    };

    /** Zero-based map layer, distinct from a tile identity or atlas cell. */
    struct LayerIndex {
        std::int64_t value{}; ///< Signed index, checked against [0, layer_count()) before storage access.

        [[nodiscard]] bool operator==(const LayerIndex&) const = default;
    };

    /** Signed map coordinates; columns increase rightward and rows downward. */
    struct Point {
        std::int64_t column{}; ///< Zero-based column; negative points are rejected by Map.
        std::int64_t row{};    ///< Zero-based row; negative points are rejected by Map.

        [[nodiscard]] bool operator==(const Point&) const = default;
    };

    /** Finite map dimensions in cells, independent of source-image dimensions. */
    struct Extent {
        std::uint32_t columns{}; ///< Number of columns; construction requires nonzero.
        std::uint32_t rows{};    ///< Number of rows; construction requires nonzero.

        [[nodiscard]] bool operator==(const Extent&) const = default;
    };

    /** Cell dimensions in caller-defined map-local units, with no rendering dependency. */
    struct CellSize {
        double width{};  ///< Finite positive width for grid queries.
        double height{}; ///< Finite positive height for grid queries.
    };

    /** Continuous map-local position; X increases rightward and Y downward, without a camera. */
    struct LocalPoint {
        double x{}; ///< Finite distance from the map's left edge, in the same map-local units as CellSize::width.
        double y{}; ///< Finite distance from the map's top edge, in the same map-local units as CellSize::height.
    };

    /** Half-open continuous rectangle in map-local X-right/Y-down units. */
    struct LocalRect {
        double left{};   ///< Finite minimum X, no greater than right.
        double top{};    ///< Finite minimum Y, no greater than bottom.
        double right{};  ///< Finite exclusive maximum X; equality with left is empty.
        double bottom{}; ///< Finite exclusive maximum Y; equality with top is empty.
    };

    /** Copied half-open cell bounds; no map, storage view or camera is retained. */
    struct CellRange {
        std::uint32_t first_column{}; ///< Inclusive first column.
        std::uint32_t end_column{};   ///< Exclusive column end, at most the extent's columns.
        std::uint32_t first_row{};    ///< Inclusive first row.
        std::uint32_t end_row{};      ///< Exclusive row end, at most the extent's rows.

        [[nodiscard]] bool empty() const noexcept { return first_column >= end_column || first_row >= end_row; }
        [[nodiscard]] bool operator==(const CellRange&) const = default;
    };

    /** Owns dense logical cells in layer-major, then row-major order.
     * Copying owns independent cells. Assignment replaces the whole value, including
     * dimensions; there is no in-place resize. Moves leave the source with zero
     * extent/layers, where get/set reject safely. A moved-from value remains assignable.
     * Confine mutations and queries to one owner, or synchronize them externally.
     * No asset, renderer, game, file, or palette is needed to construct or edit a map.
     */
    class CE_TYPE_API Map final {
        Extent extent_;
        std::uint32_t layer_count_{};
        std::vector<TileId> cells_;

        [[nodiscard]] std::size_t index(LayerIndex layer, Point point) const;

    public:
        CE_API Map(Extent extent, std::uint32_t layer_count);
        CE_API Map(const Map& other);
        CE_API Map(Map&& other) noexcept;
        CE_API Map& operator=(const Map& other);
        CE_API Map& operator=(Map&& other) noexcept;
        CE_API ~Map();

        [[nodiscard]] Extent extent() const noexcept { return extent_; }
        [[nodiscard]] std::uint32_t layer_count() const noexcept { return layer_count_; }
        [[nodiscard]] CE_API TileId get(LayerIndex layer, Point point) const;
        CE_API void set(LayerIndex layer, Point point, TileId tile);
    };

    /** Return cells whose computed rectangles share positive area with visible.
     * Cell edge i is double(i)*cell_size on each axis, including the footprint's
     * exclusive final edge. Exact touching edges are excluded using comparisons
     * against these computed edges, without division rounding or an epsilon.
     * Nonzero extents, finite positive cell sizes/footprints, increasing final cell
     * edges (as for pick_cell()) and finite ordered rectangle edges are required;
     * invalid inputs throw invalid_args.
     * Valid zero-area, outside and merely touching rectangles return CellRange{}.
     * This CPU query allocates nothing, retains nothing, and does not clip pixels.
     */
    [[nodiscard]] CE_API CellRange visible_cells(Extent extent, CellSize cell_size, LocalRect visible);

    /** Pick the cell containing a finite map-local point, or nullopt for a valid exterior point.
     * Cell edge i is double(i)*cell_size, matching visible_cells(). Internal exact edges
     * belong to the following cell; top/left are inclusive, final right/bottom exclusive.
     * Nonzero extents and finite positive sizes/footprints with increasing final cell
     * edges are required. Invalid geometry or nonfinite points throw invalid_args even
     * when exterior; all inputs remain unchanged. Positive subnormal sizes are supported.
     * With uint32_t extents and binary64 double, finite products have distinct adjacent
     * edges; no arbitrary epsilon, division-based index or linear cell scan is used.
     * Valid queries allocate/retain nothing, take logarithmic axis searches and return
     * an owned Point. World/camera conversion and layer selection belong to the caller.
     */
    [[nodiscard]] CE_API std::optional<Point> pick_cell(Extent extent, CellSize cell_size, LocalPoint point);
}

/**
 * @fn CE::Tilemaps::Map::Map(Extent extent, std::uint32_t layer_count)
 * @brief Construct a map with every logical cell empty (TileId{0}).
 * @param extent Nonzero columns and rows in cells.
 * @param layer_count Nonzero fixed number of layers.
 * @throws CE::Exceptions::invalid_args For zero axes/layers or products exceeding size_t or vector storage limits, before allocation.
 * Allocation failure propagates; construction publishes no partially usable map.
 */

/**
 * @fn CE::Tilemaps::Map::Map(const Map& other)
 * @brief Copy dimensions and cells into independent storage; allocation failure propagates.
 */

/**
 * @fn CE::Tilemaps::Map::Map(Map&& other) noexcept
 * @brief Transfer the complete value and leave other with zero extent/layers and rejected cell access.
 */

/**
 * @fn CE::Tilemaps::Map& CE::Tilemaps::Map::operator=(const Map& other)
 * @brief Replace dimensions and cells by a deep copy; allocation failure leaves this map unchanged.
 * Self-assignment preserves the value. Copying a moved-from value produces the same safely empty state.
 */

/**
 * @fn CE::Tilemaps::Map& CE::Tilemaps::Map::operator=(Map&& other) noexcept
 * @brief Replace the complete value, emptying other; self-move preserves the value.
 */

/**
 * @fn CE::Tilemaps::Map::~Map()
 * @brief Release CPU cell storage; there is no native-resource or runtime cleanup obligation.
 */

/**
 * @fn CE::Tilemaps::Extent CE::Tilemaps::Map::extent() const noexcept
 * @brief Copy cell dimensions; a moved-from map reports zero on both axes.
 */

/**
 * @fn std::uint32_t CE::Tilemaps::Map::layer_count() const noexcept
 * @brief Read the number of layers; a moved-from map reports zero.
 */

/**
 * @fn CE::Tilemaps::TileId CE::Tilemaps::Map::get(LayerIndex layer, Point point) const
 * @brief Copy one logical identity without exposing mutable storage.
 * @throws CE::Exceptions::bad_request For an invalid layer/point or access to a moved-from map.
 */

/**
 * @fn void CE::Tilemaps::Map::set(LayerIndex layer, Point point, TileId tile)
 * @brief Immediately replace one cell without allocation or palette validation; zero erases it.
 * @throws CE::Exceptions::bad_request For an invalid layer/point or access to a moved-from map; all cells remain unchanged.
 */
