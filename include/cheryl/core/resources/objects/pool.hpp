#pragma once
//#include <core/resources/allocators.h>
#include <memory>
//#include <core/resources/memory/allocators/object-pool-allocator.hpp>
#include "object-construction.hpp"

namespace CE::Obj {
    template <typename T>
    template <typename... Args>
    std::vector<std::shared_ptr<T>> PoolState<T>::retrieve_objects(std::size_t N, Args... args) {
        if (N == 0)
            return {};
        auto context = this->shared_from_this();
        std::vector<std::shared_ptr<T>> objects;
        objects.reserve(N);
        auto block = retrieve_block(N);
        std::size_t next = 0;
        // Claim slots one at a time. Each completed handle retains the pool
        // context independently of this batch and releases its own live object.
        try {
            for (; next < N; ++next) {
                auto* p = block.head.get() + next;
                // Allocate the handle before constructing T. If either step
                // fails, its false flag leaves this slot in the unclaimed tail.
                auto constructed = std::make_shared<bool>(false);
                auto handle = std::shared_ptr<T>(p, [context, constructed](T* object) noexcept {
                    if (*constructed) {
                        context->tracking_->destroy(object);
                        context->release_owned(object, 1);
                    }
                });
                tracking_->construct(p, 1, std::forward<Args>(args)...);
                *constructed = true;
                objects.push_back(std::move(handle));
            }
        } catch (...) {
            // Completed handles return their own slots as the vector unwinds.
            // The unconstructed tail remains one contiguous range.
            context->release_owned(block.head.get() + next, N - next);
            throw;
        }
        return objects;
    }

    template <typename T> void PoolState<T>::release_owned(T* p, std::size_t length) noexcept {
        try {
            return_objects(p, length);
        } catch (...) {
            std::terminate();
        }
    }

    template <typename T> Block<T> PoolState<T>::retrieve_block(std::size_t N) {
        if (N == 0) {
            throw Exceptions::bad_request(CE_HERE, "Cannot retrieve an empty object block.");
        }
        return BlockTransactions<T>::checkout(this->state_, N, std::align_val_t{alignof(T)}, [this, N] { return allocate(N); });
    }

    template <typename T> void PoolState<T>::return_objects(T* p, std::size_t length) {
        if (!BlockTransactions<T>::return_range(this->state_, p, length)) {
            throw Exceptions::bad_request(CE_HERE, "Cannot return an unknown, pooled, or out-of-bounds object range.");
        }
    }

    template <typename T> void PoolState<T>::return_block(const Block<T>& returned) {
        if (!BlockTransactions<T>::return_block(this->state_, returned)) {
            throw Exceptions::failed_operation(CE_HERE, "Object pool was returned an unknown or active block.");
        }
    }

    template <typename T> Block<T> PoolState<T>::allocate(size_t length) {
        // Borrow raw bytes from the typed memory manager. The resulting owner handle
        // destroys any tracked T objects and returns those bytes when its final alias dies.
        if (length > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw Exceptions::bad_request(CE_HERE, "The requested object storage length cannot be represented.");
        }
        auto& manager = Mem::ObjMMgr<T>::get();
        Mem::HeapBlock b = manager.checkout_chunk(length * sizeof(T), alignof(T), Enum::greedy);
        auto raw = static_cast<T*>(b.head.get());

        static_assert(!std::is_same_v<T, void>);
        const std::size_t len = b.length / sizeof(T);
        auto byte_context = manager.release_context();
        // The owner deleter runs once after every alias to this allocation
        // disappears; the slot map distinguishes constructed from raw storage.
        std::shared_ptr<T> block_root(raw, [b, len, tracking = tracking_, byte_context = std::move(byte_context)](auto p) noexcept {
            // Only tracked live slots are destroyed; unconstructed reserved slots are skipped.
            tracking->destroy(p, len);
            tracking->erase(p, p + len);
            byte_context->release_owned(b);
        });

        return {block_root, block_root, b.alignment, len};
    }
}
