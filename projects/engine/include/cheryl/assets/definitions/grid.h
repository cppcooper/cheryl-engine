#pragma once

#include <cheryl/export.h>

#include <assets/types/primitives/pixel.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace CE::Assets::Grids {
/** Zero-based row-major cell ordinal within a particular grid, not a global asset or vertex ID. */
    using CellIndex = std::size_t;

    /** Row-major cells in image pixel coordinates; origin is the top-left of the first cell.
     * cell_rect() maps an index to a source rectangle, while the grid geometry builder converts
     * that rectangle to vertices and normalized texture coordinates for the selected backend.
     * Direct fields require representable counts/coordinates; these helpers do not
     * validate image bounds or all manifest constraints. Empty axes have no cells.
     */
    struct Definition {
        CE::Rendering::Geometry::PixelPoint origin; ///< Top-left source pixel position of the first cell.
        CE::Rendering::Geometry::PixelSize frame; ///< Source width/height of each cell in pixels.
        CE::Rendering::Geometry::PixelPoint spacing; ///< Horizontal/vertical pixels between neighboring cells, excluding a trailing margin.
        std::size_t rows{}; ///< Number of grid rows; zero makes the grid empty.
        std::size_t columns{}; ///< Number of cells per row; zero makes the grid empty.

        // Count rejects size_t multiplication overflow; addressing rejects out-of-range cells.
        [[nodiscard]] CE_API std::size_t cell_count() const;
        [[nodiscard]] CE_API CellIndex cell_index(std::size_t row, std::size_t column) const;
        [[nodiscard]] CE_API CE::Rendering::Geometry::PixelRect cell_rect(CellIndex cell) const;
        [[nodiscard]] CE_API std::uint64_t occupied_right() const;
        [[nodiscard]] CE_API std::uint64_t occupied_bottom() const;
    };

/** Named rectangular grid region in cell coordinates; does not own or validate another grid. */
    struct View {
        std::string description; ///< Human-readable metadata, not a selection key.
        std::size_t row{}; ///< Zero-based starting grid row.
        std::size_t column{}; ///< Zero-based starting grid column.
        std::size_t rows{}; ///< Region height in cells.
        std::size_t columns{}; ///< Region width in cells.
    };
}

/**
 * @fn std::size_t CE::Assets::Grids::Definition::cell_count() const
 * @brief Compute rows*columns with explicit size_t overflow rejection.
 * @return Zero for an empty axis.
 * @throws CE::Exceptions::runtime_exception For an unrepresentable count; does not validate image bounds.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn CE::Assets::Grids::CellIndex CE::Assets::Grids::Definition::cell_index(std::size_t row, std::size_t column) const
 * @brief Convert a valid row/column into a row-major ordinal.
 * @param row Zero-based row less than rows.
 * @param column Zero-based column less than columns.
 * @throws CE::Exceptions::bad_request For coordinates outside the grid.
 * The caller supplies representable grid arithmetic; this helper does not independently call cell_count() to reject multiplication overflow.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn CE::Rendering::Geometry::PixelRect CE::Assets::Grids::Definition::cell_rect(CellIndex cell) const
 * @brief Compute a cell's top-left source rectangle including preceding spacing.
 * @param cell Ordinal below cell_count().
 * @return Source pixel coordinates and frame size.
 * @throws CE::Exceptions::bad_request For an out-of-range cell.
 * Count overflow also propagates; image bounds and representable coordinate arithmetic remain caller requirements.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn std::uint64_t CE::Assets::Grids::Definition::occupied_right() const
 * @brief Compute the exclusive right occupied pixel edge without a trailing gap.
 * @return origin.x for zero columns. Caller supplies representable coordinate arithmetic; no image bounds check occurs.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn std::uint64_t CE::Assets::Grids::Definition::occupied_bottom() const
 * @brief Compute the exclusive bottom occupied pixel edge without a trailing gap.
 * @return origin.y for zero rows. Caller supplies representable coordinate arithmetic; no image bounds check occurs.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */
