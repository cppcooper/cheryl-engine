#include <assets/resources/shader-asset-builder.h>

#include <internals/exceptions.h>

namespace CE::Assets::Shaders {
    std::shared_ptr<const CE::Rendering::Resources::Material>
    Builder::build_builtin_material(CE::Rendering::Materials::Builtin2D, CE::Rendering::Pipelines::Blend) {
        throw Exceptions::failed_operation(CE_HERE, "Resource provider does not support built-in 2D materials");
    }
}
