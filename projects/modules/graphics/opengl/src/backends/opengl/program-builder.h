#pragma once

#include <assets/definitions/shader-assets.h>
#include <filesystem>
#include <memory>
#include <memory_resource>
#include <vector>

namespace CE::Rendering::OpenGL {
    class ResourceLifetime;

    struct Program;

    // Private construction entry shared by the provider and native fault fixtures.
    namespace Detail {
        std::shared_ptr<Program> link_program(
            std::shared_ptr<ResourceLifetime> lifetime,
            const std::vector<std::filesystem::path>& stages,
            std::shared_ptr<std::pmr::memory_resource> logical_memory = nullptr
        );
        std::shared_ptr<Program> link_owned_program(
            std::shared_ptr<ResourceLifetime> lifetime,
            const CE::Assets::Shaders::ProgramRecipe& recipe,
            std::shared_ptr<std::pmr::memory_resource> logical_memory = nullptr
        );
    }
}
