#pragma once

#include <recovery/paired.h>

namespace CE::LivingWorld {
    struct RecoveryTestAccess {
        template<class Mutate>
        static PairedCut corrupt(const PairedCut& cut, Mutate mutate) {
            auto copy = std::make_unique<PairedCut::Data>(*cut.data_);
            mutate(*copy);
            return PairedCut(std::move(copy));
        }
        static Action action(const PairedCut& cut) { return cut.data_->action; }
        static std::uint64_t issued(const PairedCut& cut) noexcept { return cut.data_->issued; }
    };
}
