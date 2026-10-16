#pragma once

#include <cheryl/export.h>
#include <assets/resources/shader.h>
#include <glm.hpp>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

namespace CE::Assets::Shaders {
    using ProgramStore = CE::Assets::Cache::Store<CE::Rendering::Resources::Program>;
}

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Shaders {
    /** Retained linked programs under exact path keys. Load/reload on the active
     * provider/loading owner; borrowed inputs are needed only through the call.
     * Camera/draw state is mutable program access on the backend owner.
     */
    struct CE_TYPE_API ProgramCache final : ProgramStore,
                                            CE::State::Singleton<ProgramCache> {
        ProgramCache() = default;
        ~ProgramCache() override = default;
        // Existing keys skip linking; get_asset() returns a retained program or null.
        CE_API void load(
            const std::filesystem::path& key,
            const std::vector<std::filesystem::path>& stages,
            CE::Rendering::Resources::Provider& provider
        );
        // Build before replacing/inserting one entry; a throw/null program preserves
        // the previous entry. Existing readers retain the previous program generation.
        CE_API void reload(
            const std::filesystem::path& key,
            const std::vector<std::filesystem::path>& stages,
            CE::Rendering::Resources::Provider& provider
        );
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Shaders::ProgramCache>::storage() -> Storage&;
}
