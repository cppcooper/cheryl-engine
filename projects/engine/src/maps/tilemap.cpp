#include <cheryl/maps/tilemap.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace CE::Tilemaps {
    namespace {
        std::size_t storage_count(const Extent extent, const std::uint32_t layers) {
            if (extent.columns == 0 || extent.rows == 0 || layers == 0)
                throw Exceptions::invalid_args(CE_HERE, "Tilemap dimensions and layer count must be nonzero");

            auto count = static_cast<std::size_t>(extent.columns);
            for (const auto factor : {extent.rows, layers}) {
                if (factor > std::numeric_limits<std::size_t>::max() / count)
                    throw Exceptions::invalid_args(CE_HERE, "Tilemap cell count exceeds size_t");
                count *= factor;
            }
            if (count > std::vector<TileId>{}.max_size())
                throw Exceptions::invalid_args(CE_HERE, "Tilemap cell count exceeds addressable storage");
            return count;
        }

        // Find the first computed edge >= position (or > position for an upper bound).
        // Search bounded integers directly: rounded decimal division cannot decide equality.
        std::uint32_t edge_bound(const std::uint32_t count, const double size, const double position, const bool upper) {
            std::uint32_t first = 0;
            auto end = count;
            while (first < end) {
                const auto middle = first + (end - first) / 2;
                const auto edge = static_cast<double>(middle) * size;
                if (upper ? edge <= position : edge < position)
                    first = middle + 1;
                else
                    end = middle;
            }
            return first;
        }
    }

    Map::Map(const Extent extent, const std::uint32_t layer_count)
    : extent_(extent), layer_count_(layer_count), cells_(storage_count(extent, layer_count)) {}

    Map::Map(const Map& other) = default;

    Map::Map(Map&& other) noexcept
    : extent_(std::exchange(other.extent_, Extent{})),
      layer_count_(std::exchange(other.layer_count_, 0)),
      cells_(std::move(other.cells_)) {}

    Map& Map::operator=(const Map& other) {
        if (this != &other) {
            auto copied = other.cells_;
            cells_.swap(copied);
            extent_ = other.extent_;
            layer_count_ = other.layer_count_;
        }
        return *this;
    }

    Map& Map::operator=(Map&& other) noexcept {
        if (this != &other) {
            cells_ = std::move(other.cells_);
            extent_ = std::exchange(other.extent_, Extent{});
            layer_count_ = std::exchange(other.layer_count_, 0);
        }
        return *this;
    }

    Map::~Map() = default;

    std::size_t Map::index(const LayerIndex layer, const Point point) const {
        if (layer.value < 0 || layer.value >= layer_count_ || point.column < 0 || point.row < 0 || point.column >= extent_.columns ||
            point.row >= extent_.rows)
            throw Exceptions::bad_request(CE_HERE, "Tilemap layer or cell coordinates are out of bounds");
        return (static_cast<std::size_t>(layer.value) * extent_.rows + static_cast<std::size_t>(point.row)) * extent_.columns +
               static_cast<std::size_t>(point.column);
    }

    TileId Map::get(const LayerIndex layer, const Point point) const {
        return cells_[index(layer, point)];
    }

    void Map::set(const LayerIndex layer, const Point point, const TileId tile) {
        cells_[index(layer, point)] = tile;
    }

    CellRange visible_cells(const Extent extent, const CellSize cell_size, const LocalRect visible) {
        if (extent.columns == 0 || extent.rows == 0 || !std::isfinite(cell_size.width) || !std::isfinite(cell_size.height) ||
            cell_size.width <= 0.0 || cell_size.height <= 0.0)
            throw Exceptions::invalid_args(CE_HERE, "Visible cells require nonzero extents and finite positive cell sizes");

        const auto width = static_cast<double>(extent.columns) * cell_size.width;
        const auto height = static_cast<double>(extent.rows) * cell_size.height;
        if (!std::isfinite(width) || !std::isfinite(height))
            throw Exceptions::invalid_args(CE_HERE, "Tilemap footprint is not finite");
        if (!std::isfinite(visible.left) || !std::isfinite(visible.top) || !std::isfinite(visible.right) ||
            !std::isfinite(visible.bottom) || visible.left > visible.right || visible.top > visible.bottom)
            throw Exceptions::invalid_args(CE_HERE, "Visible rectangle must have finite ordered edges");

        const auto left = std::max(0.0, visible.left);
        const auto top = std::max(0.0, visible.top);
        const auto right = std::min(width, visible.right);
        const auto bottom = std::min(height, visible.bottom);
        if (left >= right || top >= bottom)
            return {};

        return {
            edge_bound(extent.columns, cell_size.width, left, true) - 1, edge_bound(extent.columns, cell_size.width, right, false),
            edge_bound(extent.rows, cell_size.height, top, true) - 1, edge_bound(extent.rows, cell_size.height, bottom, false)
        };
    }
}
