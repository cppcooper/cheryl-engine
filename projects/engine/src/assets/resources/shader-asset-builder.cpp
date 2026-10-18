#include <assets/resources/shader-asset-builder.h>
#include <assets/resources/resource-provider.h>

#include <internals/exceptions.h>

namespace CE::Assets::Shaders {
    std::shared_ptr<const CE::Rendering::Resources::Material> build_builtin_material(
        CE::Rendering::Resources::Provider& provider,
        const CE::Rendering::Materials::Builtin2D kind,
        const CE::Rendering::Pipelines::Blend blend
    ) {
        auto* builder = provider.shader_asset_builder();
        if (!builder)
            throw Exceptions::failed_operation(CE_HERE, "Resource provider does not support built-in 2D materials");
        auto material = builder->build_builtin_material(kind, blend);
        if (!material)
            throw Exceptions::failed_operation(CE_HERE, "Resource provider returned a missing built-in 2D material");
        return material;
    }
    std::shared_ptr<const CE::Rendering::Resources::Material>
    Builder::build_builtin_material(CE::Rendering::Materials::Builtin2D, CE::Rendering::Pipelines::Blend) {
        throw Exceptions::failed_operation(CE_HERE, "Resource provider does not support built-in 2D materials");
    }
}
