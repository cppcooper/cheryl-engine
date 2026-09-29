#include <core/resources/asset-management/shader-mgr.h>

#include <assets/resources/resource-provider.h>

namespace CE::Assets {
    void ShaderMgr::load_program(const std::filesystem::path& key,
                                 const std::vector<std::filesystem::path>& stages,
                                 ResourceProvider& provider) {
        bind_provider(provider);
        if (loaded_assets.contains(key))
            return;
        // Link once and seed the current hard-coded sampler/camera/model uniforms
        // before publishing this program in the cache.
        auto program = provider.link_program(stages);
        program->use();
        // TODO: Move engine-standard uniform names and sampler defaults into a material/pipeline
        // description instead of teaching the generic ShaderMgr one shader naming convention.
        program->set_uniform_value("mytexture", 0);
        program->set_uniform_matrix("projectionMatrix", projection_);
        program->set_uniform_matrix("viewMatrix", view_);
        program->set_uniform_matrix("modelMatrix", glm::mat4(1.0f));
        loaded_assets.emplace(key, std::move(program));
    }

    void ShaderMgr::clear_assets() noexcept {
        ShaderAssetMgr::clear_assets();
        projection_ = glm::mat4(1.0f);
        view_ = glm::mat4(1.0f);
    }

    void ShaderMgr::set_projection_matrix(const glm::mat4& projection) {
        set_camera_matrices(projection, view_);
    }

    void ShaderMgr::set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) {
        // TODO: Revisit broadcasting camera state through the singleton shader cache. Binding
        // frame/pass state when a program is submitted would avoid mutating every cached program
        // whenever the active camera changes.
        projection_ = projection;
        view_ = view;
        for (const auto& entry : loaded_assets) {
            const auto& program = entry.second;
            program->use();
            program->set_uniform_matrix("projectionMatrix", projection_);
            program->set_uniform_matrix("viewMatrix", view_);
        }
    }
}
