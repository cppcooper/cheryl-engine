#include <assets/resources/resource-provider.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>

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
        const auto attempt = Diagnostics::next_domain_id();
        try {
            bind_provider(provider);
            auto program = provider.link_program(stages);
            if (!program)
                throw Exceptions::failed_operation(CE_HERE, "Resource provider returned no shader program");
            // Cache only executable resources. A successful replacement is published after linking.
            replace_asset(key, std::move(program));
        } catch (...) {
            Diagnostics::report_outcome("assets", attempt, "program_reload", "preserved_previous");
            CE_LOG_ERROR(
                CE::assetlog, "subsystem=assets domain={} cache={} operation=program_reload outcome=preserved_previous", attempt, domain_
            );
            throw;
        }
        CE_LOG_DEBUG(
            CE::assetlog, "subsystem=assets domain={} cache={} operation=program_reload outcome=published publications={} replacements={}",
            attempt, domain_, diagnostics().publications, diagnostics().replacements
        );
    }
}
