#include <resources/assets/tileset-mgr.h>
#include <internals.h>

namespace CE::Assets {
    void TilesetMgr::load_assets(const std::vector<fs::path> &files) {
        const auto N = files.size();
        auto assets = allocate<Tileset>(N);
        for(int i = 0; i < N; ++i) {
            const auto &file = files[i];
            if (!loaded_assets.contains(file)) {
                const auto &asset = assets[i];
                Obj::ObjCtor<Tileset>::construct(asset.get(), 1, Tileset::load_tilset(file));
                loaded_assets[file] = asset;
            }
        }
    }
}
