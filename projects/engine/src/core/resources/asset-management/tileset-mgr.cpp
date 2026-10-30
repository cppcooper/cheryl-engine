#include <core/resources/asset-management/tileset-mgr.h>

#include <assets/geometry/grid-geometry.h>
#include <assets/resources/resource-provider.h>
#include <assets/resources/shader-asset-builder.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/shader-asset-mgr.h>
#include <core/resources/objects/object-construction.hpp>
#include <internals/exceptions.h>

#include <algorithm>
#include <utility>

namespace CE::Assets::Tilesets {
    void Manager::load(const std::vector<Definition>& definitions, CE::Rendering::Resources::Provider& provider) {
        bind_provider(provider);
        // Reserve for new IDs only; a repeated load does not replace a cached tileset.
        std::vector<const Definition*> pending;
        for (const auto& definition : definitions)
            if (!contains(definition.id()))
                pending.push_back(&definition);
        auto reservation = reserve<Asset>(pending.size());
        std::size_t slot = 0;
        for (const auto* selected : pending) {
            const auto& definition = *selected;
            const auto id = definition.id();
            if (contains(id)) {
                continue;
            }
            const auto texture = CE::Assets::Images::Manager::get().get_asset(definition.texture);
            if (!texture) {
                throw Exceptions::runtime_exception(
                    CE_HERE, "Tileset '" + id + "' references an unloaded texture '" + definition.texture.string() + "'"
                );
            }
            const auto texture_size = texture->pixel_size();
            if (texture_size.width == 0 || texture_size.height == 0) {
                throw Exceptions::runtime_exception(CE_HERE, "Tileset '" + id + "' has an empty texture");
            }
            const auto material =
                definition.shader
                    ? CE::Assets::Shaders::Manager::get().find_material(*definition.shader)
                    : CE::Assets::Shaders::build_builtin_material(provider, CE::Rendering::Materials::Builtin2D::TexturedStrip);
            if (definition.shader && !material)
                throw Exceptions::runtime_exception(CE_HERE, "Tileset '" + id + "' references an unloaded shader '" + *definition.shader + "'");
            // Translate each grid cell into vertices, upload once, then retain geometry and
            // unconsumed animation/autotile metadata in the cached tileset.
            auto geometry = CE::Assets::Grids::make_geometry(definition.grid, definition.pivot, texture_size);
            auto mesh = provider.upload_geometry(
                std::move(geometry.vertices), geometry.vertex_count, CE::Rendering::Geometry::Topology::TriangleStrip
            );
            if (material) {
                CE::Rendering::Pipelines::PassConstraints2D unconstrained;
                unconstrained.depth.reset();
                material->definition().pipeline->validate_draw(*mesh, 0, mesh->vertex_count(), unconstrained);
            }
            auto asset = reservation.emplace(
                slot++, Data{.geometry = std::move(mesh), .texture = texture, .definition = definition, .material = material}
            );
            publish_asset(id, std::move(asset));
        }
    }
}

namespace CE::State {
    template <> auto Singleton<CE::Assets::Tilesets::Manager>::storage() -> Storage& {
        static Storage value;
        return value;
    }
}
