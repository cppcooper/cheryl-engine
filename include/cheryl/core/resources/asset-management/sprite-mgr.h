#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/2d/sprite.h>

using SMgr = CE::Assets::AssetMgr<CE::Assets::Sprite, std::string>;
namespace CE::Assets {
    struct ResourceProvider;

    struct SpriteMgr final : SMgr, Singleton_CTS<SpriteMgr> {
        SpriteMgr() = default;
        ~SpriteMgr() override = default;
        void load_assets(const std::vector<SpriteDefinition>& definitions, ResourceProvider& provider);
    };
}
