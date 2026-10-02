#pragma once

#include <assets/types/primitives/pixel.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace CE::Assets {
    using CellIndex = std::size_t;

    /** Row-major cells in image pixel coordinates; origin is the top-left of the first cell.
     * cell_rect() maps an index to a source rectangle, while the grid geometry builder converts
     * that rectangle to vertices and normalized texture coordinates for the selected backend.
     */
    struct GridDefinition {
        PixelPoint origin;
        PixelSize frame;
        PixelPoint spacing;
        std::size_t rows{};
        std::size_t columns{};

        [[nodiscard]] std::size_t cell_count() const;
        [[nodiscard]] CellIndex cell_index(std::size_t row, std::size_t column) const;
        [[nodiscard]] PixelRect cell_rect(CellIndex cell) const;
        [[nodiscard]] std::uint64_t occupied_right() const;
        [[nodiscard]] std::uint64_t occupied_bottom() const;
    };

    struct ViewDefinition {
        std::string description;
        std::size_t row{};
        std::size_t column{};
        std::size_t rows{};
        std::size_t columns{};
    };
}
