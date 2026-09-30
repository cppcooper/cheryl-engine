#include <assets/resources/resource-provider.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <internals/exceptions.h>

namespace CE::Assets {
    void ShaderMgr::load_program(
        const std::filesystem::path& key,
        const std::vector<std::filesystem::path>& stages,
        ResourceProvider& provider
    ) {
        bind_provider(provider);
        if (contains(key))
            return;
        reload_program(key, stages, provider);
    }

    void ShaderMgr::reload_program(
        const std::filesystem::path& key,
        const std::vector<std::filesystem::path>& stages,
        ResourceProvider& provider
    ) {
        bind_provider(provider);
        auto program = provider.link_program(stages);
        if (!program)
            throw Exceptions::failed_operation(CE_HERE, "Resource provider returned no shader program");
        // Cache only executable resources. A successful replacement is published after linking.
        replace_asset(key, std::move(program));
    }
}
