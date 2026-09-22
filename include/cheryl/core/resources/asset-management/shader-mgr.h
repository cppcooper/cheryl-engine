#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/primitives/glslprogram.h>
#include <glm.hpp>

using GLSLMgr = CE::Assets::AssetMgr<CE::Assets::GLSLProgram>;
namespace CE::Assets {
    struct ShaderMgr final : GLSLMgr, Singleton_CTS<ShaderMgr> {
        ShaderMgr() = default;
        ~ShaderMgr() override = default;
        void load_assets(const std::vector<std::filesystem::path>&);
        void load_program(const std::filesystem::path& key, const std::vector<std::filesystem::path>& stages);
        void set_projection_matrix(const glm::mat4& projection);
        void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view);

    private:
        glm::mat4 projection_{1.0f};
        glm::mat4 view_{1.0f};
        std::vector<std::filesystem::path> linked_programs_;
    };
}
