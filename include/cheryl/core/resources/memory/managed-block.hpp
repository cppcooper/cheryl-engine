#pragma once
#include <core/resources/memory/mem-mgr.h>
#include <memory>

namespace CE::Mem {
    // The handle owns the backing block. Returning it to a live manager enables
    // reuse; if the manager has already died, the block frees its backing normally.
    template<typename T, typename Manager>
    std::shared_ptr<T> make_managed_block(Manager& manager, HeapBlock block) {
        auto* head = static_cast<T*>(block.head.get());
        auto lifetime = manager.lifetime_token();
        return std::shared_ptr<T>(head, [block = std::move(block), lifetime, manager_ptr = &manager](T*) noexcept {
            if (lifetime.lock()) manager_ptr->return_chunk(block);
        });
    }
}
