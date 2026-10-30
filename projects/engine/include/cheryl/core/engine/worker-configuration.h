#pragma once

#include <cheryl/export.h>

#include <core/engine/worker-pool.h>

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace CE::Workers {
/** Inert owned pool/group settings; validation creates neither threads nor groups.
     * @see [Worker configuration](cheryl-guide:runtime/worker-execution)
     */
    struct Configuration {
        PoolOptions pool; ///< Pool capacity/provider settings applied only when a pool is constructed.
        std::map<std::string, GroupOptions, std::less<>> groups; ///< Named group options; names match [a-z][a-z0-9_]* and do not create groups automatically.
    };

    // Names start with a lowercase ASCII letter and contain lowercase letters,
    // digits and underscores. Configuration is inert until the caller creates work.
    [[nodiscard]] CE_API bool is_group_name(std::string_view name) noexcept;
    CE_API void validate_configuration(const Configuration& configuration);
}

/**
 * @fn bool CE::Workers::is_group_name(std::string_view name) noexcept
 * @brief Check the portable worker-group name syntax.
 * @param name Borrowed candidate; empty is invalid.
 * @return True for a lowercase ASCII letter followed only by lowercase letters, digits or underscores.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn void CE::Workers::validate_configuration(const Configuration& configuration)
 * @brief Validate names and structural group options without applying native policy.
 * @param configuration Borrowed inert settings, unchanged on return or failure.
 * @throws CE::Exceptions::invalid_args For invalid names/options or required NUMA placement.
 * Does not discover topology, validate a provider at runtime, create a pool or open groups.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */
