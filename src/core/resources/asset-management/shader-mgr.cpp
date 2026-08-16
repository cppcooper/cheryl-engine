#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/objects/object-construction.hpp>
#include <internals.h>
#include <core/rendering/opengl-renderer.h>

namespace CE::Assets {
    void ShaderMgr::load_assets(const std::vector<fs::path> &files) {
        static auto& renderer = Singleton_CTS<RenderAPIs::OpenGLRenderer>::get();
        const auto N = files.size();
        auto assets = allocate<GLSLProgram>(N);
        // Obj::ObjCtor<GLSLProgram>::construct(assets[0].get(),N);
        for(int i = 0; i < N; ++i) {
            const auto &file = files[i];
            if (!loaded_assets.contains(file)) {
                const auto &asset = assets[i];
                Obj::ObjCtor<GLSLProgram>::construct(asset.get(),1,renderer.compile_shader(file));
                loaded_assets[file] = asset;
            }
        }
    }
}
