#pragma once

#include <filesystem>
#include <memory>
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
            const std::vector<std::filesystem::path>& stages
        );
    }
}
