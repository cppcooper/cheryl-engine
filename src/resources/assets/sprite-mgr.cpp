#include <resources/assets/sprite-mgr.h>
#include <internals.h>

namespace CE::Assets {
    void SpriteMgr::load_assets(const std::vector<fs::path> &files) {
        const auto N = files.size();
        auto assets = allocate<Sprite>(N);
        for(int i = 0; i < N; ++i) {
            const auto &file = files[i];
            if (!loaded_assets.contains(file)) {
                const auto &asset = assets[i];
                Obj::ObjCtor<Sprite>::construct(asset.get(), 1, Sprite::load_sprite(file));
                loaded_assets[file] = asset;
            }
        }
    }
}
