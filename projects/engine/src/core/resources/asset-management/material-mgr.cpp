#include <core/resources/asset-management/material-mgr.h>

#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>

#include <utility>

namespace CE::Assets {
    void MaterialMgr::load_material(const std::filesystem::path& key, ResourceProvider& provider, const Builder& builder) {
        bind_provider(provider);
        if (!contains(key))
            reload_material(key, provider, builder);
    }

    void MaterialMgr::reload_material(const std::filesystem::path& key, ResourceProvider& provider, const Builder& builder) {
        const auto attempt = Diagnostics::next_domain_id();
        try {
            bind_provider(provider);
            if (!builder)
                throw Exceptions::invalid_args(CE_HERE, "Material reload needs an explicit recipe builder");
            auto candidate = builder(provider);
            if (!candidate)
                throw Exceptions::failed_operation(CE_HERE, "Material builder returned no recipe");
            // Linking, reflection, defaults, and images are validated by the builder
            // before this one publication point. Existing frame owners keep the old
            // complete material/pipeline pair; no published generation is mutated.
            replace_asset(key, std::move(candidate));
        } catch (...) {
            Diagnostics::report_outcome("assets", attempt, "material_reload", "preserved_previous");
            CE_LOG_ERROR(
                CE::assetlog, "subsystem=assets domain={} cache={} operation=material_reload outcome=preserved_previous", attempt, domain_
            );
            throw;
        }
        CE_LOG_DEBUG(
            CE::assetlog, "subsystem=assets domain={} cache={} operation=material_reload outcome=published publications={} replacements={}",
            attempt, domain_, diagnostics().publications, diagnostics().replacements
        );
    }
}
