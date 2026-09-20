#include <assets/manifest.h>

#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace CE::Assets {
    std::size_t WangSignatureHash::operator()(const WangSignature& signature) const noexcept {
        std::size_t result = 1469598103934665603ULL;
        for (const auto value : signature) {
            result ^= value;
            result *= 1099511628211ULL;
        }
        return result;
    }

    std::size_t GridDefinition::cell_count() const {
        if (rows != 0 && columns > std::numeric_limits<std::size_t>::max() / rows) {
            throw std::overflow_error("Asset grid cell count exceeds size_t");
        }
        return rows * columns;
    }

    CellIndex GridDefinition::cell_index(const std::size_t row, const std::size_t column) const {
        if (row >= rows || column >= columns) {
            throw std::out_of_range("Asset grid cell coordinates are out of range");
        }
        return row * columns + column;
    }

    PixelRect GridDefinition::cell_rect(const CellIndex cell) const {
        if (cell >= cell_count()) {
            throw std::out_of_range("Asset grid cell index is out of range");
        }
        const auto row = cell / columns;
        const auto column = cell % columns;
        return {origin.x + column * (static_cast<std::uint64_t>(frame.width) + spacing.x),
                origin.y + row * (static_cast<std::uint64_t>(frame.height) + spacing.y), frame.width, frame.height};
    }

    std::uint64_t GridDefinition::occupied_right() const {
        if (columns == 0) {
            return origin.x;
        }
        return origin.x + columns * static_cast<std::uint64_t>(frame.width) +
            (columns - 1) * static_cast<std::uint64_t>(spacing.x);
    }

    std::uint64_t GridDefinition::occupied_bottom() const {
        if (rows == 0) {
            return origin.y;
        }
        return origin.y + rows * static_cast<std::uint64_t>(frame.height) +
            (rows - 1) * static_cast<std::uint64_t>(spacing.y);
    }

    std::string SpriteDefinition::id() const {
        return name_space + ':' + name;
    }

    std::string TilesetDefinition::id() const {
        return name_space + ':' + name;
    }

    std::vector<std::filesystem::path> AssetManifest::textures() const {
        std::vector<std::filesystem::path> result;
        std::unordered_set<std::filesystem::path> seen;
        const auto append = [&result, &seen](const std::filesystem::path& path) {
            if (seen.emplace(path).second) {
                result.push_back(path);
            }
        };
        for (const auto& sprite : sprites) {
            append(sprite.texture);
        }
        for (const auto& tileset : tilesets) {
            append(tileset.texture);
        }
        return result;
    }
}
