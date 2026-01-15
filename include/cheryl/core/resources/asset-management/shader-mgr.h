#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/primitives/glslprogram.h>

using GLSLMgr = CE::Assets::AssetMgr<CE::Assets::GLSLProgram>;
namespace CE::Assets {
    struct ShaderMgr final : GLSLMgr, Singleton_CTS<ShaderMgr> {
        ShaderMgr() = default;
        ~ShaderMgr() override = default;
        void load_assets(const std::vector<std::filesystem::path>&) override;
    };
}
