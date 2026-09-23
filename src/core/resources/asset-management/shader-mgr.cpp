#include <core/resources/asset-management/shader-mgr.h>

#include <assets/abstracts/resource-provider.h>

namespace CE::Assets {
    void ShaderMgr::load_assets(const std::vector<std::filesystem::path>& files, ResourceProvider& provider) {
        bind_provider(provider);
        for (const auto& file : files) {
            if (!loaded_assets.contains(file)) {
                loaded_assets.emplace(file, provider.compile_stage(file));
            }
        }
    }

    void ShaderMgr::load_program(const std::filesystem::path& key,
                                 const std::vector<std::filesystem::path>& stages,
                                 ResourceProvider& provider) {
        bind_provider(provider);
        if (loaded_assets.contains(key))
            return;
        auto program = provider.link_program(stages);
        program->use();
        program->set_uniform_value("mytexture", 0);
        program->set_uniform_matrix("projectionMatrix", projection_);
        program->set_uniform_matrix("viewMatrix", view_);
        program->set_uniform_matrix("modelMatrix", glm::mat4(1.0f));
        loaded_assets.emplace(key, std::move(program));
        linked_programs_.push_back(key);
    }

    void ShaderMgr::set_projection_matrix(const glm::mat4& projection) {
        set_camera_matrices(projection, view_);
    }

    void ShaderMgr::set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) {
        projection_ = projection;
        view_ = view;
        for (const auto& key : linked_programs_) {
            auto program = loaded_assets.at(key);
            program->use();
            program->set_uniform_matrix("projectionMatrix", projection_);
            program->set_uniform_matrix("viewMatrix", view_);
        }
    }
}
