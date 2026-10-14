#include <core/engine/worker-configuration.h>
#include <internals/exceptions.h>

#include <algorithm>

namespace CE::Workers {
    bool is_group_name(const std::string_view name) noexcept {
        if (name.empty() || name.front() < 'a' || name.front() > 'z')
            return false;
        return std::ranges::all_of(name, [](const char value) {
            return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '_';
        });
    }

    void validate_configuration(const Configuration& configuration) {
        for (const auto& [name, options] : configuration.groups) {
            if (!is_group_name(name))
                throw Exceptions::invalid_args(CE_HERE, "Worker group name '" + name + "' must match [a-z][a-z0-9_]*");
            try {
                validate_group_options(options);
            } catch (const Exceptions::invalid_args& error) {
                throw Exceptions::invalid_args(CE_HERE, "Worker group '" + name + "': " + error.what());
            }
            if (options.cpu.strength == PolicyStrength::Required && options.cpu.numa_node)
                throw Exceptions::invalid_args(CE_HERE, "Worker group '" + name + "' requires unavailable NUMA placement");
        }
    }
}
