#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/primitives/texture.h>
#include <cgl.h>

#define TEXTURE_MANAGER_MAX_TEXTURES 32

using TMgr = CE::Assets::AssetMgr<CE::Assets::Texture>;
namespace CE::Assets {
    struct TextureMgr final : TMgr, Singleton_CTS<TextureMgr> {
        using texid = GLuint;
        TextureMgr() = default;
        ~TextureMgr() override = default;
        void change_default_slot(uint32_t slot = GL_TEXTURE0);
        texid get_active(const uint32_t slot = GL_TEXTURE0) const { return Bound_IDs[slot - GL_TEXTURE0]; }
        void update_bind(uint32_t slot, texid id);
        void load_assets(const std::vector<std::filesystem::path>&) override;
    protected:
        int32_t default_bind_slot = GL_TEXTURE0;
        bool useMipMaps = true;
        texid Bound_IDs[TEXTURE_MANAGER_MAX_TEXTURES]{}; //currently bound texture
    };
}
