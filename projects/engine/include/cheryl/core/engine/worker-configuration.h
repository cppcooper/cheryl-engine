#pragma once

#include <cheryl/export.h>

#include <core/engine/worker-pool.h>

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace CE::Workers {
    struct Configuration {
        PoolOptions pool;
        std::map<std::string, GroupOptions, std::less<>> groups;
    };

    // Names start with a lowercase ASCII letter and contain lowercase letters,
    // digits and underscores. Configuration is inert until the caller creates work.
    [[nodiscard]] CE_API bool is_group_name(std::string_view name) noexcept;
    CE_API void validate_configuration(const Configuration& configuration);
}
