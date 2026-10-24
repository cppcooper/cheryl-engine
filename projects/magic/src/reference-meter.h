#pragma once

#include <magic/reference-host.h>

namespace Magic::Reference {
    struct ReferenceHost::Meter {
        const HostLimits& limits;
        Usage usage;
        std::uint64_t action = 0;
        std::uint64_t tick = 0;
        explicit Meter(const HostLimits& value)
        : limits(value) {
            spend(292); // Prior280 +4 remains/4 servants/4 retained plan handles (inline candidate copy).
        }
        [[noreturn]] void fail(Error code) const { throw Failure{code, action, tick}; }
        void spend(std::size_t count = 1) {
            if (count > limits.work - usage.work)
                fail(Error::WorkLimit);
            usage.work += count;
        }
        void visit() {
            spend();
            if (usage.selector_work == limits.selector_work)
                fail(Error::SelectorLimit);
            ++usage.selector_work;
        }
        void select() {
            spend();
            if (usage.selected_targets == limits.selected_targets)
                fail(Error::TargetLimit);
            ++usage.selected_targets;
        }
    };

}
