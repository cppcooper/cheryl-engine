#pragma once
#include <templates/block.h>
#include <templates/singleton.h>
#include <core/resources/memory/mem-mgr.h>

namespace CE::Obj {
    /**
     * Owns the object pool's release operations and retains shared block
     * bookkeeping. Handles hold this context so their last release can merge
     * a slot back into the pool after the Pool<T> facade is destroyed.
     */
    template<typename T>
    struct PoolState : AbstractManager<T>, std::enable_shared_from_this<PoolState<T>> {
        static_assert(std::is_class_v<T>, "Pool<T> must have a class for T");

        /** Construct N objects; each returned handle retains this release state. */
        template<typename... Args>
        std::vector<std::shared_ptr<T>> retrieve_objects(std::size_t N, Args... args);
        /** Reserve N unconstructed slots in one Block. */
        Block<T> retrieve_block(std::size_t N);
        void return_objects(T* p, std::size_t length);
        void return_block(const Block<T>& returned);

        /**
         * Release a known owned range from a destructor. Invalid bookkeeping
         * terminates rather than letting an exception escape a deleter.
         */
        void release_owned(T* p, std::size_t length) noexcept;

    private:
        [[nodiscard]] static Block<T> allocate(std::size_t length);
    };

    /**
     * Singleton entry point for allocating objects. Existing manager methods
     * remain available; release_context() gives handles a retained PoolState.
     */
    template<typename T>
    struct Pool : AbstractManager<T>, Singleton_CTS<Pool<T>> {
        using release_context_type = PoolState<T>;

        Pool() : context_(std::make_shared<release_context_type>()) {}
        /** Retain the release state without requiring a later singleton lookup. */
        [[nodiscard]] std::shared_ptr<release_context_type> release_context() const { return context_; }

        template<typename... Args>
        std::vector<std::shared_ptr<T>> retrieve_objects(std::size_t N, Args... args) {
            return context_->retrieve_objects(N, std::forward<Args>(args)...);
        }
        Block<T> retrieve_block(std::size_t N) { return context_->retrieve_block(N); }
        void return_objects(T* p, std::size_t length) { context_->return_objects(p, length); }
        void return_block(const Block<T>& block) { context_->return_block(block); }
        void cull(std::chrono::minutes age) override { context_->cull(age); }
        void release_culled() override { context_->release_culled(); }

    private:
        std::shared_ptr<release_context_type> context_;
    };
}

#include "pool.hpp"
