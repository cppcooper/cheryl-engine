#pragma once

#include <cheryl/export.h>
#include <assets/resources/decoded-image.h>
#include <assets/resources/image.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Images {
    using Store = CE::Assets::Cache::Store<CE::Rendering::Resources::Image>;

    struct CE_TYPE_API Manager final : Store,
                                       CE::State::Singleton<Manager> {
        Manager() = default;
        ~Manager() override = default;
        // Exact lexically normalized path first; a bare filename may uniquely match
        // another path. Missing returns null, ambiguity throws. contains() is exact-key only.
        [[nodiscard]] CE_API Handle get_asset(const std::filesystem::path& file) const override;
        // Serialize on the cache/provider loading owner. Existing normalized keys
        // are reused without rereading/replacing their immutable image contents.
        // Batch failure can leave earlier entries; input pixels are not retained.
        CE_API void load(const std::vector<std::filesystem::path>& files, CE::Rendering::Resources::Provider& provider);
        CE_API void upload(const std::filesystem::path& file, const Decoded& image, CE::Rendering::Resources::Provider& provider);
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Images::Manager>::storage() -> Storage&;
}
