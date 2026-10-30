#pragma once
#include <core/resources/memory/mem-mgr.h>
#include <memory>

namespace CE::Memory {
    /**
     * Retain a HeapBlock and its byte release context. Final release returns the
     * range for reuse even after the Manager facade has been destroyed. Creating
     * the handle requires a live facade; releasing it does not access that facade.
     * @tparam T Pointer element type; this helper neither constructs nor destroys T objects.
     * @tparam Manager Facade exposing a retained byte release_context().
     * @param manager Live facade borrowed only to capture its release context.
     * @param block Valid checked-out byte range, consumed into the final deleter.
     * @return Shared pointer to the range head, retaining backing owner/context.
     * Retained bytes are not synchronized for user reads/writes. Do not separately
     * return the same range while any returned handle remains. Handle-allocation
     * failure may invoke the prepared final deleter to return the range.
     * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
     */
    template <typename T, typename Manager> std::shared_ptr<T> make_managed_block(Manager& manager, HeapBlock block) {
        auto* head = static_cast<T*>(block.head.get());
        auto context = manager.release_context();
        return std::shared_ptr<T>(head, [block = std::move(block), context = std::move(context)](T*) noexcept {
            context->release_owned(block);
        });
    }
}
