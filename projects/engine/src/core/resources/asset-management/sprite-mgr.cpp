#include <core/resources/asset-management/sprite-mgr.h>

#include <assets/geometry/grid-geometry.h>
#include <assets/resources/resource-provider.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/shader-asset-mgr.h>
#include <core/resources/objects/object-construction.hpp>
#include <internals/exceptions.h>

#include <algorithm>
#include <utility>

namespace CE::Assets::Sprites {
    Manager::Handle Manager::get_asset(const std::string_view name_space, const std::string_view name) const {
        return get_asset(std::string(name_space) + ':' + std::string(name));
    }

    std::map<std::string, Manager::Handle, std::less<>> Manager::get_assets(const std::string_view name_space) const {
        std::map<std::string, Handle, std::less<>> result;
        std::shared_lock lock(assets_mutex_);
        for (const auto& [id, sprite] : loaded_assets) {
            const auto& definition = sprite->definition();
            if (definition.name_space == name_space)
                result.emplace(definition.name, sprite);
        }
        return result;
    }

    void Manager::load(const std::vector<Definition>& definitions, CE::Rendering::Resources::Provider& provider) {
        bind_provider(provider);
        // Reserve only slots for new IDs; existing assets keep their handles across repeated loads.
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
                    CE_HERE, "Sprite '" + id + "' references an unloaded texture '" + definition.texture.string() + "'"
                );
            }
            const auto texture_size = texture->pixel_size();
            if (texture_size.width == 0 || texture_size.height == 0) {
                throw Exceptions::runtime_exception(CE_HERE, "Sprite '" + id + "' has an empty texture");
            }
            const auto material = definition.shader ? CE::Assets::Shaders::Manager::get().find_material(*definition.shader) : nullptr;
            if (definition.shader && !material)
                throw Exceptions::runtime_exception(CE_HERE, "Sprite '" + id + "' references an unloaded shader '" + *definition.shader + "'");
            // Build CPU vertices from manifest grid/pivot, upload them through the selected backend,
            // and transfer the claimed slot from this reservation into the cache.
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
