#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/2d/sprite.h>

using SMgr = CE::Assets::AssetMgr<CE::Assets::Sprite>;
namespace CE::Assets {
    struct SpriteMgr final : SMgr, Singleton_CTS<SpriteMgr> {
        SpriteMgr() = default;
        ~SpriteMgr() override = default;
        void load_assets(const std::vector<std::filesystem::path>&) override;
    };
}
