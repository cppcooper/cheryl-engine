#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/objects/object-construction.hpp>
#include <internals.h>

namespace CE::Assets {
    void ShaderMgr::load_assets(const std::vector<fs::path> &files) {
        const auto N = files.size();
        auto assets = allocate<GLSLProgram>(N);
        Obj::ObjCtor<GLSLProgram>::construct(assets[0].get(),N);
        for(int i = 0; i < N; ++i) {
            const auto &file = files[i];
            if (!loaded_assets.contains(file)) {
                const auto &asset = assets[i];
                asset->compileShaderFromFile(file.c_str(), GLSLShader::get_type(file.extension().string()));
                loaded_assets[file] = asset;
            }
        }
    }
}
