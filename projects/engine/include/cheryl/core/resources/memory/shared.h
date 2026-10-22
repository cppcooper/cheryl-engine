#pragma once

#include <memory>

namespace CE::Memory {
    /** Standard shared ownership alias; copying retains the same control block.
     * Pointee synchronization and resource-domain lifetime remain T's responsibility.
     */
    template <typename T> using Shared = std::shared_ptr<T>;
}
