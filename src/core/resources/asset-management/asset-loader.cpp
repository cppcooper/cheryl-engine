#include <templates/asset-mgr.h>
#include <core/resources/asset-management.h>
#include <core/resources/fileio/fonts-system.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace CE::Assets {
    void Loader::load_assets() {
        const std::array<iAssetMgr*,5> managers {
            &TextureMgr::get(),
            &TilesetMgr::get(),
            &SpriteMgr::get(),
            &FontMgr::get(),
            &ShaderMgr::get()
        };
        const std::vector<fspath> &jsons = get_files_of_type("json");
        std::vector<fspath> sprites, tilesets;
        for(const auto& file : jsons) {
            std::ifstream fstream(file);
            using json = nlohmann::json;
            if (json data = json::parse(fstream); !data.empty()) {
                if(!data["animations"].empty()) {
                    sprites.push_back(file);
                }
                if(!data["tileset"].empty()) {
                    tilesets.push_back(file);
                }
            }
        }
        auto vert = get_files_of_type(".vert");
        auto geo = get_files_of_type(".geo");
        auto frag = get_files_of_type(".frag");
        auto tesc = get_files_of_type(".tesc");
        auto tese = get_files_of_type(".tese");
        std::vector<fspath> shaders;
        shaders.reserve(vert.size() + geo.size() + frag.size() + tesc.size() + tese.size());
        shaders.insert( shaders.end(), vert.begin(), vert.end() );
        shaders.insert( shaders.end(), geo.begin(), geo.end() );
        shaders.insert( shaders.end(), frag.begin(), frag.end() );
        shaders.insert( shaders.end(), tesc.begin(), tesc.end() );
        shaders.insert( shaders.end(), tese.begin(), tese.end() );

        // todo: parameterize desired fonts
        std::unordered_set<std::string> valid_fonts{
            "arial.ttf",
            "calibri.ttf",
            "consola.ttf",
            "ProggyVector Regular.ttf"
        };
        auto ffont = get_files_of_type(".fdat");
        std::vector<fspath> fonts = Resources::find_system_fonts();
        std::vector<fspath> ok_fonts;
        for(auto f : fonts) {
            if(valid_fonts.contains(f)) {
                ok_fonts.push_back(f);
            }
        }
        ok_fonts.insert(ok_fonts.end(), ffont.begin(), ffont.end());
        const std::array file_lists {
            get_files_of_type(".png"),
            tilesets, sprites,
            ok_fonts,
            shaders
        };
        for(int i = 0; i < 4; ++i) {
            managers[i]->load_assets(file_lists[i]);
        }
    }
}
