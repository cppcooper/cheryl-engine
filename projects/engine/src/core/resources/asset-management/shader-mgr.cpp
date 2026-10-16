#include <assets/resources/resource-provider.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>

namespace CE::Assets::Shaders {
    void ProgramCache::load(
        const std::filesystem::path& key,
        const std::vector<std::filesystem::path>& stages,
        CE::Rendering::Resources::Provider& provider
    ) {
        bind_provider(provider);
        if (contains(key))
            return;
        reload(key, stages, provider);
    }

    void ProgramCache::reload(
        const std::filesystem::path& key,
        const std::vector<std::filesystem::path>& stages,
        CE::Rendering::Resources::Provider& provider
    ) {
        const auto attempt = CE::Diagnostics::next_domain_id();
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
                CE::Logging::Categories::assets, "subsystem=assets domain={} cache={} operation=program_reload outcome=preserved_previous",
                attempt, domain_
            );
            throw;
        }
        CE_LOG_DEBUG(
            CE::Logging::Categories::assets,
            "subsystem=assets domain={} cache={} operation=program_reload outcome=published publications={} replacements={}", attempt,
            domain_, diagnostics().publications, diagnostics().replacements
        );
    }
}

namespace CE::State {
    template <> auto Singleton<CE::Assets::Shaders::ProgramCache>::storage() -> Storage& {
        static Storage value;
        return value;
    }
}
