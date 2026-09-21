#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/objects/object-construction.hpp>
#include <internals.h>
#include <core/rendering/opengl-renderer.h>
#include <memory>

namespace CE::Assets {
    void ShaderMgr::load_assets(const std::vector<fs::path>& files) {
        static auto& renderer = Singleton_CTS<RenderAPIs::OpenGLRenderer>::get();
        const auto N = files.size();
        auto assets = allocate<GLSLProgram>(N);
        // Obj::ObjCtor<GLSLProgram>::construct(assets[0].get(),N);
        for (std::size_t i = 0; i < N; ++i) {
            const auto& file = files[i];
            if (!loaded_assets.contains(file)) {
                const auto& asset = assets[i];
                Obj::ObjCtor<GLSLProgram>::construct(asset.get(), 1, renderer.compile_shader(file));
                loaded_assets[file] = asset;
            }
        }
    }

    void ShaderMgr::load_program(const fs::path& key, const std::vector<fs::path>& stages) {
        if (loaded_assets.contains(key))
            return;
        auto& renderer = Singleton_CTS<RenderAPIs::OpenGLRenderer>::get();
        auto program = std::make_shared<GLSLProgram>(static_cast<int>(renderer.compile_program(stages)));
        program->use();
        program->set_uniform_value("mytexture", GLint{0});
        program->set_uniform_matrix("projectionMatrix", projection_);
        program->set_uniform_matrix("viewMatrix", glm::mat4(1.0f));
        program->set_uniform_matrix("modelMatrix", glm::mat4(1.0f));
        loaded_assets.emplace(key, std::move(program));
        linked_programs_.push_back(key);
    }

    void ShaderMgr::set_projection_matrix(const glm::mat4& projection) {
        projection_ = projection;
        for (const auto& key : linked_programs_) {
            auto program = loaded_assets.at(key);
            program->use();
            program->set_uniform_matrix("projectionMatrix", projection_);
        }
    }
}
