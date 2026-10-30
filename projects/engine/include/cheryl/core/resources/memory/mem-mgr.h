#pragma once
#include "typedefs.h"
#include "release-context.h"
#include <enums.h>
#include <templates/singleton.h>
#include <chrono>
#include <bit>

namespace CE::Memory {
    /** One coherent shared byte-domain snapshot, independent of formatting/logging. */
    struct Stats {
        ::CE::Diagnostics::DomainId domain = 0; ///< Shared void-block bookkeeping identity across growth-policy facades.
        std::size_t total_bytes = 0; ///< Bytes in recorded backing owners, including checked-out and reusable ranges.
        std::size_t available_bytes = 0; ///< Bytes in reusable pool records.
        std::size_t owners = 0; ///< Recorded full backing allocations.
        std::size_t ranges = 0; ///< Recorded split sections, not a count of live user objects.
        std::size_t pending_release = 0; ///< Whole-owner records queued for release_culled().
    };
    /** Recycles byte ranges, aligning/growing a request before checkout and tracking owners,
     * sections, and pooled remainders through CE::Memory::AbstractBlockManager<void>. Public operations
     * serialize bookkeeping transitions across all collections; backing allocation,
     * final frees, and reporting run outside those locks. Concurrent operations require
     * a live facade or a previously retained release context. This does not synchronize
     * user access to checked-out bytes or make raw collection edits safe.
     */
    template <double growth_factor_ = 1.6, int32_t growth_base_ = 256>
    struct Manager : AbstractBlockManager<void>,
                     ::CE::State::Singleton<Manager<growth_factor_, growth_base_>> {
        static_assert(growth_base_ >= 0, "The base growth should be a positive integer.");
        static_assert(growth_factor_ > 0, "The growth factor cannot be 0.");

    private:
        std::shared_ptr<void> lifetime_token_ = std::make_shared<int>(0);
        const std::shared_ptr<::CE::Memory::ByteReleaseContext> release_context_ = std::make_shared<::CE::Memory::ByteReleaseContext>();

    public:
        /** Retained return interface that may outlive this facade. */
        using release_context_type = ::CE::Memory::ByteReleaseContext;

        Manager() = default;
        ~Manager() override { lifetime_token_.reset(); }
        // Legacy liveness observation only; it neither pins the facade nor synchronizes teardown.
        [[nodiscard]] std::weak_ptr<void> lifetime_token() const { return lifetime_token_; }
        // Capture while the facade is live; final release may then outlive its destruction.
        [[nodiscard]] std::shared_ptr<release_context_type> release_context() const { return release_context_; }
        // Shared byte-domain statistics; free percentage is n/a when total bytes are zero.
        [[nodiscard]] std::string stats();
        // Shared byte-domain snapshot; no formatting/logging while collections lock.
        [[nodiscard]] ::CE::Memory::Stats diagnostics() const;
        // Explicit unlocked observer; the host chooses its sampling interval.
        void report_diagnostics() const noexcept;
        // retrieve debug info
        [[nodiscard]] std::string debug_info();
        // return all ownership of a section containing ptr
        void return_ptr(void* ptr);
        // return a portion of a block
        void return_portion(void* ptr, std::size_t length);
        // return all ownership of a section - this argument is presumed to exist in `sections`
        void return_chunk(const ::CE::Memory::HeapBlock& returned);
        // retrieve an allocation with the given length (+alignment) - returns a section of an allocation at least big enough to fill the request
        [[nodiscard]] ::CE::Memory::HeapBlock checkout_chunk(
            size_t length,
            size_t alignment = 0,
            ::CE::Memory::FitPolicy fit = ::CE::Memory::exact,
            size_t growth_base = growth_base_,
            double growth_factor = growth_factor_
        );

    private:
        // retrieve an allocation with the given length - returns a section of an allocation at least big enough to fill the request
        [[nodiscard]] static ::CE::Memory::HeapBlock allocate(size_t length, std::align_val_t alignment) {
            alignment = static_cast<std::align_val_t>(std::bit_ceil(static_cast<std::size_t>(alignment)));
            auto deleter = [alignment](void* p) { ::operator delete[](p, alignment); };
            const auto ptr = std::shared_ptr<void>(::operator new[](length, alignment), deleter);
            return {ptr, ptr, ::CE::Memory::Pointers::calculate_alignment(ptr.get()), length};
        }

    public:
        // add (b1 * w2) new heap allocations aligned to a3
        void preallocate(
            size_t blocks,
            size_t width,
            size_t gb = growth_base_,
            double gf = growth_factor_,
            std::align_val_t alignment = std::align_val_t{64}
        );
    };

    /** Manager whose default growth adds no spare bytes; it still reuses the shared byte pool. */
    using ExactManager = Manager<1.0, 0>;
    /** Manager with default 4x request growth plus 4096 bytes, over the shared byte pool. */
    using CachedManager = Manager<4.0, 4096>;
    /** Byte-manager growth policy scaled for T; lengths remain bytes and no T construction occurs. */
    template <typename T> using ObjectMemoryManager = Manager<1.29, sizeof(T) * 64>;
}

#include "mem-mgr.hpp"

/**
 * @fn template<double growth_factor_, int32_t growth_base_> CE::Memory::Manager<growth_factor_, growth_base_>::Manager()
 * @brief Create a live byte-manager facade and retained release context over shared void bookkeeping.
 * @tparam growth_factor_ Positive default multiplier used only by greedy growth.
 * @tparam growth_base_ Nonnegative default added-byte allowance.
 * No backing allocation occurs until checkout/preallocation. Different growth-policy specializations share the same byte domain.
 */

/**
 * @fn template<double growth_factor_, int32_t growth_base_> CE::Memory::Manager<growth_factor_, growth_base_>::~Manager()
 * @brief Expire the legacy facade liveness token. Previously captured byte release contexts continue to return retained ranges without dereferencing this facade; quiesce direct facade callers before destruction.
 */

/**
 * @fn template<double growth_factor_, int32_t growth_base_> std::weak_ptr<void> CE::Memory::Manager<growth_factor_, growth_base_>::lifetime_token() const
 * @return Weak legacy liveness observation; it neither pins this facade nor synchronizes concurrent teardown. Capture only while the facade is live.
 */

/**
 * @fn template<double growth_factor_, int32_t growth_base_> std::shared_ptr<CE::Memory::ByteReleaseContext> CE::Memory::Manager<growth_factor_, growth_base_>::release_context() const
 * @return Retained shared byte-domain return context; capture while this facade lives, then final returns may outlive it. No user-byte synchronization is implied.
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */
