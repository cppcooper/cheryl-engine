#pragma once
#include <assets/resources/shader.h>
#include <glm.hpp>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

using ShaderAssetMgr = CE::Assets::AssetMgr<CE::Assets::Shader>;
namespace CE::Assets {
    struct ResourceProvider;

    /** Cache linked programs by path. Camera/draw state is bound at submission. */
    struct ShaderMgr final : ShaderAssetMgr,
                             Singleton_CTS<ShaderMgr> {
        ShaderMgr() = default;
        ~ShaderMgr() override = default;
        void load_program(const std::filesystem::path& key, const std::vector<std::filesystem::path>& stages, ResourceProvider& provider);
        // Replacing a cache entry leaves previously published frames' handles alive.
        void reload_program(const std::filesystem::path& key, const std::vector<std::filesystem::path>& stages, ResourceProvider& provider);
    };
}
