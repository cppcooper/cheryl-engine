#pragma once

#include <core/engine/worker-pool.h>

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace CE::Engine {
    struct WorkerConfiguration {
        WorkerPoolOptions pool;
        std::map<std::string, WorkerGroupOptions, std::less<>> groups;
    };

    // Names start with a lowercase ASCII letter and contain lowercase letters,
    // digits and underscores. Configuration is inert until the caller creates work.
    [[nodiscard]] bool is_worker_group_name(std::string_view name) noexcept;
    void validate_worker_configuration(const WorkerConfiguration& configuration);
}
