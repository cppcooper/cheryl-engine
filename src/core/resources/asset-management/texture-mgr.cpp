#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/objects/object-construction.hpp>
#include <internals.h>

namespace CE::Assets {
    void TextureMgr::change_default_slot(uint32_t slot) {
        default_bind_slot = slot;
    }

    void TextureMgr::update_bind(const uint32_t slot, const texid id) {
        Bound_IDs[slot-GL_TEXTURE0] = id;
    }

    void TextureMgr::load_assets(const std::vector<fs::path> &files) {
        const auto N = files.size();
        auto assets = allocate<Texture>(N);
        for(int i = 0; i < N; ++i) {
            const auto &file = files[i];
            if (!loaded_assets.contains(file)){
                const auto &asset = assets[i];
                Obj::ObjCtor<Texture>::construct(asset.get(), 1,
                    file.string().c_str(), default_bind_slot, useMipMaps, false, GL_CLAMP_TO_EDGE);
                loaded_assets[file] = asset;
            } // unused assets[i], i.e. not in loaded_assets,
            // will be reclaimed as per their shared ptr nature
        }
    }
}

