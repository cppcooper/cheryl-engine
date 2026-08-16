#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/2d/tileset.h>

using TSMgr = CE::Assets::AssetMgr<CE::Assets::Tileset>;
namespace CE::Assets {
    struct TilesetMgr final : TSMgr, Singleton_CTS<TilesetMgr> {
        TilesetMgr() = default;
        ~TilesetMgr() override = default;
        void load_assets(const std::vector<std::filesystem::path>&) override;
    };
}
