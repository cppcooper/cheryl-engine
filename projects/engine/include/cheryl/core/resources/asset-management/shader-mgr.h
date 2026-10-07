#pragma once
#include <assets/resources/shader.h>
#include <glm.hpp>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

using ShaderAssetMgr = CE::Assets::AssetMgr<CE::Assets::Shader>;
namespace CE::Assets {
    struct ResourceProvider;

    /** Retained linked programs under exact path keys. Load/reload on the active
     * provider/loading owner; borrowed inputs are needed only through the call.
     * Camera/draw state is mutable program access on the backend owner.
     */
    struct ShaderMgr final : ShaderAssetMgr,
                             Singleton_CTS<ShaderMgr> {
        ShaderMgr() = default;
        ~ShaderMgr() override = default;
        // Existing keys skip linking; get_asset() returns a retained program or null.
        void load_program(const std::filesystem::path& key, const std::vector<std::filesystem::path>& stages, ResourceProvider& provider);
        // Build before replacing/inserting one entry; a throw/null program preserves
        // the previous entry. Existing readers retain the previous program generation.
        void reload_program(const std::filesystem::path& key, const std::vector<std::filesystem::path>& stages, ResourceProvider& provider);
    };
}
