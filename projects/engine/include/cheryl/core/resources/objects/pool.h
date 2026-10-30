#pragma once
#include <templates/block.h>
#include <templates/singleton.h>
#include <core/resources/memory/mem-mgr.h>
#include "object-construction.hpp"

namespace CE::Memory::Objects {
    /**
     * Owns the object pool's release operations and retains shared block
     * bookkeeping. Handles hold this context so their last release can merge
     * a slot back into the pool after the Pool<T> facade is destroyed.
     */
    template <typename T>
    struct PoolState : ::CE::Memory::AbstractBlockManager<T>,
                       std::enable_shared_from_this<PoolState<T>> {
        static_assert(std::is_class_v<T>, "Pool<T> must have a class for T");

    private:
        const std::shared_ptr<typename ::CE::Memory::Objects::Constructor<T>::Context> tracking_ =
            ::CE::Memory::Objects::Constructor<T>::release_context();

    public:
        [[nodiscard]] std::shared_ptr<typename ::CE::Memory::Objects::Constructor<T>::Context> construction_context() const {
            return tracking_;
        }
        template <typename... Args> std::vector<std::shared_ptr<T>> retrieve_objects(std::size_t N, Args... args);
        ::CE::Memory::Block<T> retrieve_block(std::size_t N);
        void return_objects(T* p, std::size_t length);
        void return_block(const ::CE::Memory::Block<T>& returned);

        void release_owned(T* p, std::size_t length) noexcept;

    private:
        [[nodiscard]] ::CE::Memory::Block<T> allocate(std::size_t length);
    };

    /**
     * Singleton entry point for allocating objects. Existing manager methods
     * remain available; release_context() gives handles a retained PoolState.
     */
    template <typename T>
    struct Pool : ::CE::Memory::AbstractBlockManager<T>,
                  ::CE::State::Singleton<Pool<T>> {
        /** Retained typed pool/tracker context used by final object owners. */
        using release_context_type = ::CE::Memory::Objects::PoolState<T>;

        Pool()
        : context_(std::make_shared<release_context_type>()) {}
        [[nodiscard]] std::shared_ptr<release_context_type> release_context() const { return context_; }

        template <typename... Args> std::vector<std::shared_ptr<T>> retrieve_objects(std::size_t N, Args... args) {
            return context_->retrieve_objects(N, std::forward<Args>(args)...);
        }
        ::CE::Memory::Block<T> retrieve_block(std::size_t N) { return context_->retrieve_block(N); }
        void return_objects(T* p, std::size_t length) { context_->return_objects(p, length); }
        void return_block(const ::CE::Memory::Block<T>& block) { context_->return_block(block); }
        void cull(std::chrono::minutes age) override { context_->cull(age); }
        void release_culled() override { context_->release_culled(); }

    private:
        std::shared_ptr<release_context_type> context_;
    };
}

#include "pool.hpp"

/**
 * @fn template<typename T> std::shared_ptr<typename CE::Memory::Objects::Constructor<T>::Context> CE::Memory::Objects::PoolState<T>::construction_context() const
 * @return Retained per-T construction tracker. Final owners capture it before facade teardown; it synchronizes slot transitions, not user operations on T.
 */

/**
 * @fn template<typename T> CE::Memory::Objects::Pool<T>::Pool()
 * @brief Create a facade with a shared-owned PoolState over the per-T block domain.
 * @tparam T Class type whose objects fit representable aligned storage.
 * The facade itself need not outlive handles that captured release_context(); concurrent direct facade use still requires it to remain alive.
 */

/**
 * @fn template<typename T> std::shared_ptr<typename CE::Memory::Objects::Pool<T>::release_context_type> CE::Memory::Objects::Pool<T>::release_context() const
 * @return Retained typed pool/release state captured while this facade is live. Later construction/final release may use it without looking up the singleton.
 */

/**
 * @fn template<typename T> template<typename... Args> auto CE::Memory::Objects::Pool<T>::retrieve_objects(std::size_t N, Args... args) -> std::vector<std::shared_ptr<T>>
 * @brief Delegate batch construction to the retained pool state.
 * @param N Object count; zero returns no handles.
 * @param args Stored constructor arguments forwarded for each object; repeated forwarding can reuse already moved-from values.
 * @return Independent per-object handles retaining the release state, which may outlive this facade.
 * Construction/allocation failure destroys completed objects and returns remaining storage; see PoolState::retrieve_objects().
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */

/**
 * @fn template<typename T> CE::Memory::Block<T> CE::Memory::Objects::Pool<T>::retrieve_block(std::size_t N)
 * @brief Reserve a raw checked-out block through the retained context.
 * @param N Positive object-slot count.
 * @return Unconstructed storage metadata, potentially enlarged by byte growth/alignment. No objects are constructed; return the range only after users/lifetimes end.
 */

/**
 * @fn template<typename T> void CE::Memory::Objects::Pool<T>::return_objects(T* p, std::size_t length)
 * @brief Delegate a positive in-bounds slot-range return; this does not itself destroy objects.
 * @param p First slot no longer in use.
 * @param length Number of T slots, not bytes.
 * Unknown/already-pooled/out-of-bounds returns throw bad_request. Destroy live objects through the compatible tracker before returning raw storage.
 */

/**
 * @fn template<typename T> void CE::Memory::Objects::Pool<T>::return_block(const CE::Memory::Block<T>& block)
 * @brief Return an exact current block to the retained typed pool state.
 * @param block Checked-out metadata with no remaining live users.
 * Invalid returns throw failed_operation; object construction/destruction remains a separate responsibility.
 */

/**
 * @fn template<typename T> void CE::Memory::Objects::Pool<T>::cull(std::chrono::minutes age)
 * @brief Queue complete free owners old enough for later release.
 * @param age Minimum stale age; does not force checked-out/split owners free.
 * No final backing destruction occurs until release_culled().
 */

/**
 * @fn template<typename T> void CE::Memory::Objects::Pool<T>::release_culled()
 * @brief Remove still-complete free queued owners transactionally, then destroy final backing owners after unlocking. Retained user ranges are not forcibly invalidated.
 */
