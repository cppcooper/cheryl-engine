#pragma once
#include <templates/block.h>
#include <templates/singleton.h>
#include <core/resources/memory/mem-mgr.h>

namespace CE::Obj {
    template<typename T>
    struct PoolState : AbstractManager<T>, std::enable_shared_from_this<PoolState<T>> {
        static_assert(std::is_class_v<T>, "Pool<T> must have a class for T");

        template<typename... Args>
        std::vector<std::shared_ptr<T>> retrieve_objects(std::size_t N, Args... args);
        Block<T> retrieve_block(std::size_t N);
        void return_objects(T* p, std::size_t length);
        void return_block(const Block<T>& returned);

        // Trusted, already owned ranges are released through this path in destructors.
        // An invariant violation is fatal here; exceptions never leave a deleter.
        void release_owned(T* p, std::size_t length) noexcept;

    private:
        [[nodiscard]] static Block<T> allocate(std::size_t length);
    };

    template<typename T>
    struct Pool : AbstractManager<T>, Singleton_CTS<Pool<T>> {
        using release_context_type = PoolState<T>;

        Pool() : context_(std::make_shared<release_context_type>()) {}
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
