#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/abstracts/shader.h>
#include <glm.hpp>

using ShaderAssetMgr = CE::Assets::AssetMgr<CE::Assets::Shader>;
namespace CE::Assets {
    struct ResourceProvider;

    /** Cache compiled stages and linked programs by path; linked programs receive the
     * current camera matrices when they are loaded or when the camera changes.
     */
    struct ShaderMgr final : ShaderAssetMgr, Singleton_CTS<ShaderMgr> {
        ShaderMgr() = default;
        ~ShaderMgr() override = default;
        void load_assets(const std::vector<std::filesystem::path>& files, ResourceProvider& provider);
        void load_program(const std::filesystem::path& key, const std::vector<std::filesystem::path>& stages,
                          ResourceProvider& provider);
        void set_projection_matrix(const glm::mat4& projection);
        void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view);

    private:
        glm::mat4 projection_{1.0f};
        glm::mat4 view_{1.0f};
        std::vector<std::filesystem::path> linked_programs_;
    };
}
