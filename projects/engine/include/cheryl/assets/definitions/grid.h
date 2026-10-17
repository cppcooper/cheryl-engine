#pragma once

#include <cheryl/export.h>

#include <assets/types/primitives/pixel.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace CE::Assets::Grids {
    using CellIndex = std::size_t;

    /** Row-major cells in image pixel coordinates; origin is the top-left of the first cell.
     * cell_rect() maps an index to a source rectangle, while the grid geometry builder converts
     * that rectangle to vertices and normalized texture coordinates for the selected backend.
     * Direct fields require representable counts/coordinates; these helpers do not
     * validate image bounds or all manifest constraints. Empty axes have no cells.
     */
    struct Definition {
        CE::Rendering::Geometry::PixelPoint origin;
        CE::Rendering::Geometry::PixelSize frame;
        CE::Rendering::Geometry::PixelPoint spacing;
        std::size_t rows{};
        std::size_t columns{};

        // Count rejects size_t multiplication overflow; addressing rejects out-of-range cells.
        [[nodiscard]] CE_API std::size_t cell_count() const;
        [[nodiscard]] CE_API CellIndex cell_index(std::size_t row, std::size_t column) const;
        [[nodiscard]] CE_API CE::Rendering::Geometry::PixelRect cell_rect(CellIndex cell) const;
        [[nodiscard]] CE_API std::uint64_t occupied_right() const;
        [[nodiscard]] CE_API std::uint64_t occupied_bottom() const;
    };

    struct View {
        std::string description;
        std::size_t row{};
        std::size_t column{};
        std::size_t rows{};
        std::size_t columns{};
    };
}
