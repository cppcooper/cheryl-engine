#include <assets/submission/tilemap2d.h>

#include <assets/types/primitives/vertex.h>
#include <internals/exceptions.h>

#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace CE::Rendering::Submission {
    namespace {
        void validate_axis(
            const std::uint32_t origin,
            const std::uint32_t frame,
            const std::uint32_t spacing,
            const std::size_t count,
            const std::uint32_t limit
        ) {
            // Compare before multiplication: even directly constructed huge grids cannot wrap.
            if (origin > limit || frame > limit - origin ||
                count - 1 > (limit - origin - frame) / (static_cast<std::uint64_t>(frame) + spacing))
                throw Exceptions::invalid_args(CE_HERE, "Tilemap source grid exceeds its atlas image");
        }

        void validate_source(const CE::Assets::Tilesets::Asset& asset, const CE::Assets::Grids::CellIndex target) {
            const auto& definition = asset.definition();
            const auto& grid = definition.grid;
            const auto pivot = definition.pivot;
            if (grid.rows == 0 || grid.columns == 0 || grid.frame.width == 0 || grid.frame.height == 0 ||
                grid.columns > std::numeric_limits<std::size_t>::max() / grid.rows || !std::isfinite(pivot.x) || !std::isfinite(pivot.y) ||
                pivot.x < 0 || pivot.x > 1 || pivot.y < 0 || pivot.y > 1)
                throw Exceptions::invalid_args(CE_HERE, "Tilemap source grid/frame/pivot is invalid");
            const auto count = grid.cell_count();
            if (target >= count || count > std::numeric_limits<std::size_t>::max() / Geometry::vertices_per_strip_quad)
                throw Exceptions::invalid_args(CE_HERE, "Tilemap source target or vertex count is invalid");
            if (!asset.texture)
                throw Exceptions::invalid_args(CE_HERE, "Tilemap source needs an atlas image");
            const auto image = asset.texture->pixel_size();
            validate_axis(grid.origin.x, grid.frame.width, grid.spacing.x, grid.columns, image.width);
            validate_axis(grid.origin.y, grid.frame.height, grid.spacing.y, grid.rows, image.height);
            for (const auto& [name, clip] : definition.animations) {
                if (clip.target >= count)
                    throw Exceptions::invalid_args(CE_HERE, "Tilemap animation target is outside the grid: " + name);
                static_cast<void>(clip.cell_at(std::chrono::milliseconds{})); // Authoritative timeline validation, no mutation.
                for (const auto& frame : clip.frames)
                    if (frame.cell >= count)
                        throw Exceptions::invalid_args(CE_HERE, "Tilemap animation frame is outside the grid: " + name);
            }
        }

        float checked_float(const double value, const bool positive = false) {
            const auto limit = static_cast<double>(std::numeric_limits<float>::max());
            if (!std::isfinite(value) || value < -limit || value > limit)
                throw Exceptions::invalid_args(CE_HERE, "Tilemap transform is not representable as finite floats");
            const auto result = static_cast<float>(value);
            if (positive && result <= 0)
                throw Exceptions::invalid_args(CE_HERE, "Tilemap transform scale must remain positive as a float");
            return result;
        }

        void validate_matrix(const glm::mat4& matrix) {
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    if (!std::isfinite(matrix[column][row]))
                        throw Exceptions::invalid_args(CE_HERE, "Tilemap pass matrices must be finite");
        }

        glm::mat4 cell_model(const CE::Assets::Tilesets::Definition& definition, const CE::Tilemaps::Point point, const TilemapView& view) {
            const auto pivot = definition.pivot;
            const auto frame = definition.grid.frame;
            glm::mat4 model{1.0f};
            model[0][0] = checked_float(view.cell_size.width / frame.width, true);
            model[1][1] = checked_float(view.cell_size.height / frame.height, true);
            model[3][0] = checked_float(view.origin.x + (static_cast<double>(point.column) + pivot.x) * view.cell_size.width);
            model[3][1] = checked_float(view.origin.y - (static_cast<double>(point.row) + pivot.y) * view.cell_size.height);

            // Match the source geometry's float pivot arithmetic; finite matrix entries
            // alone do not guarantee finite transformed corners near float limits.
            const auto fw = static_cast<float>(frame.width);
            const auto fh = static_cast<float>(frame.height);
            for (const auto x : {-pivot.x * fw, (1.0f - pivot.x) * fw})
                for (const auto y : {(pivot.y - 1.0f) * fh, pivot.y * fh}) {
                    const auto corner = model * glm::vec4{x, y, 0, 1};
                    if (!std::isfinite(corner.x) || !std::isfinite(corner.y))
                        throw Exceptions::invalid_args(CE_HERE, "Tilemap transformed source corners are not finite");
                }
            return model;
        }
    }

    void TilemapBindings::add(
        const CE::Tilemaps::TileId id,
        std::shared_ptr<const CE::Assets::Tilesets::Asset> asset,
        const CE::Assets::Grids::CellIndex target
    ) {
        if (id.value == 0 || bindings_.contains(id.value) || !asset)
            throw Exceptions::invalid_args(CE_HERE, "Tilemap binding needs a unique nonzero ID and a tileset");
        validate_source(*asset, target);
        bindings_.emplace(id.value, Binding{std::move(asset), target});
    }

    std::vector<DrawPacket2D> resolve_tilemap_layer(
        const CE::Tilemaps::Map& map,
        const CE::Tilemaps::LayerIndex layer,
        const TilemapBindings& bindings,
        const TilemapView& view,
        const DrawStyle2D& style,
        const Context2D& context
    ) {
        if (layer.value < 0 || layer.value >= map.layer_count())
            throw Exceptions::bad_request(CE_HERE, "Tilemap submission layer is out of bounds");
        if (view.elapsed.count() < 0 || style.scale != 1.0f || style.model_matrix != glm::mat4{1.0f})
            throw Exceptions::invalid_args(CE_HERE, "Tilemap submission needs nonnegative time, identity base model and unit style scale");
        validate_matrix(context.pass.projection);
        validate_matrix(context.pass.view);
        if (style.clip)
            validate_clip_region(*style.clip);
        const auto extent = map.extent();
        const auto range = CE::Tilemaps::visible_cells(extent, view.cell_size, view.visible);
        static_cast<void>(checked_float(view.origin.x));
        static_cast<void>(checked_float(view.origin.y));
        static_cast<void>(checked_float(view.cell_size.width, true));
        static_cast<void>(checked_float(view.cell_size.height, true));
        static_cast<void>(checked_float(view.origin.x + static_cast<double>(extent.columns) * view.cell_size.width));
        static_cast<void>(checked_float(view.origin.y - static_cast<double>(extent.rows) * view.cell_size.height));

        std::vector<DrawPacket2D> packets;
        for (auto row = range.first_row; row < range.end_row; ++row)
            for (auto column = range.first_column; column < range.end_column; ++column) {
                const CE::Tilemaps::Point point{column, row};
                const auto id = map.get(layer, point);
                if (id.value == 0)
                    continue;
                const auto found = bindings.bindings_.find(id.value);
                if (found == bindings.bindings_.end())
                    throw Exceptions::bad_request(
                        CE_HERE, "Tilemap missing logical ID " + std::to_string(id.value) + " at layer " + std::to_string(layer.value) +
                                     " cell (" + std::to_string(column) + "," + std::to_string(row) + ")"
                    );
                const auto& binding = found->second;
                auto placed = style;
                placed.model_matrix = cell_model(binding.asset->definition(), point, view);
                const auto final_cell = binding.asset->cell_at(binding.target, view.elapsed);
                packets.push_back(resolve_tile(*binding.asset, final_cell, placed, context));
            }
        return packets;
    }
}
