#pragma once

#include <cheryl/export.h>

#include <memory>
#include <optional>

namespace CE::Workers::Hwloc::Testing {
    // Test observations execute in the owner without exposing or linking hwloc
    // in the runner. Snapshots retain the exact native node bitmap and policy.
    class MemoryPolicy {
        std::shared_ptr<const void> native_;
        friend class MemoryProbe;

        CE_WORKERS_API explicit MemoryPolicy(std::shared_ptr<const void> native);

    public:
        [[nodiscard]] CE_WORKERS_API int policy() const noexcept;
        [[nodiscard]] CE_WORKERS_API bool same_nodes(const MemoryPolicy& other) const noexcept;
    };

    class MemoryProbe {
        std::shared_ptr<void> native_;

    public:
        CE_WORKERS_API MemoryProbe();
        [[nodiscard]] CE_WORKERS_API std::optional<MemoryPolicy> capture() const;
    };
}
