#pragma once

#include <assets/definitions/shader-assets.h>
#include <filesystem>
#include <memory>
#include <memory_resource>
#include <vector>

namespace CE::RenderAPIs {
    class OpenGLResourceLifetime;
}

namespace CE::Assets {
    struct GLSLProgram;

    // Private construction entry shared by the provider and native fault fixtures.
    namespace ProgramDetail {
        std::shared_ptr<GLSLProgram> link_program(
            std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
            const std::vector<std::filesystem::path>& stages,
            std::shared_ptr<std::pmr::memory_resource> logical_memory = nullptr
        );
        std::shared_ptr<GLSLProgram> link_owned_program(
            std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
            const ShaderProgramRecipe& recipe,
            std::shared_ptr<std::pmr::memory_resource> logical_memory = nullptr
        );
    }
}
