#pragma once

#include <cheryl/tools/console/registry.h>

namespace CE::Tools::Console::Detail {
    struct Entry {
        Descriptor descriptor;
        std::function<std::shared_ptr<Work>(const Context&, std::span<const Value>)> handler;
        bool active = true;
    };
    struct RegistryState {
        std::vector<std::weak_ptr<Entry>> entries;
    };
}
