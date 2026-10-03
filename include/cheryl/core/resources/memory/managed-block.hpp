#pragma once
#include <core/resources/memory/mem-mgr.h>
#include <memory>

namespace CE::Mem {
    /**
     * Retain a HeapBlock and its byte release context. Final release returns the
     * range for reuse even after the Manager facade has been destroyed. Creating
     * the handle requires a live facade; releasing it does not access that facade.
     */
    template <typename T, typename Manager> std::shared_ptr<T> make_managed_block(Manager& manager, HeapBlock block) {
        auto* head = static_cast<T*>(block.head.get());
        auto context = manager.release_context();
        return std::shared_ptr<T>(head, [block = std::move(block), context = std::move(context)](T*) noexcept {
            context->release_owned(block);
        });
    }
}
