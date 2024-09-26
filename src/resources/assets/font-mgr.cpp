#include <resources/assets/font-mgr.h>
#include <assets/2d/ffont.h>
#include <assets/2d/stbfont.h>
#include <internals.h>

namespace CE::Assets {
    void FontMgr::load_assets(const std::vector<fs::path> &files) {
        const auto N = files.size();
        auto assets = allocate<STBFont>(N);
        for(int i = 0; i < N; ++i) {
            const auto &file = files[i];
            if (!loaded_assets.contains(file)) {
                auto &asset = assets[i];
                if (file.filename() != "font.fdat") [[likely]] {
                    Obj::ObjCtor<STBFont>::construct(asset.get(),1,STBFont::load_font(file.c_str(),12));
                    loaded_assets[file] = asset;
                } else {
                    // if the file is font.fdat this is our manual font (just a png)
                    FFont::get(FFont::load_ffont(file));
                    loaded_assets[file] = std::shared_ptr<FFont>(&FFont::get(),[](void*){});
                }
            }
        }
    }
}
