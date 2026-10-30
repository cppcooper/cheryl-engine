#pragma once

/** @file
 * @brief Template definitions for byte checkout, return and diagnostics; include mem-mgr.h.
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */
#include "mem-mgr.h"
#include <internals/celog.h>
#include <math/fit.h>
#include <math/bytes.h>
#include <optional>
#include <utility>
#include <format>

namespace CE::Memory {
    /** Copy shared byte-domain counts under coordinated collection read locks.
     * @return Consistent backing/pool/section/release counters; no native calls or logging.
     */
    template <double gf_, int32_t gb_> Stats Manager<gf_, gb_>::diagnostics() const {
        Stats result{this->state_->domain};
        std::shared_lock l1(get_mutex(pool), std::defer_lock);
        std::shared_lock l2(get_mutex(sections), std::defer_lock);
        std::shared_lock l3(get_mutex(registry), std::defer_lock);
        std::shared_lock l4(get_mutex(release), std::defer_lock);
        std::lock(l1, l2, l3, l4);
        for (const auto& b : std::get<1>(this->pool))
            result.available_bytes += b.length;
        for (const auto& b : std::get<1>(this->registry))
            result.total_bytes += b.length;
        result.owners = std::get<1>(this->registry).size();
        result.ranges = std::get<1>(this->sections).size();
        result.pending_release = std::get<1>(this->release).size();
        return result;
    }

    /** Emit an optional DEBUG observation after obtaining a synchronized snapshot.
     * The host chooses sampling frequency; disabled logging may avoid sampling.
     * No bookkeeping locks are held while formatting or invoking the logger.
     */
    template <double gf_, int32_t gb_> void Manager<gf_, gb_>::report_diagnostics() const noexcept {
        ::CE::Logging::Logger<::CE::Logging::Categories::memory>::template write_lazy<::CE::Logging::Compile::DEBUG_>([&](auto& log) {
            const auto observed = diagnostics();
            log.debug("subsystem=memory domain={} operation=summary total_bytes={} available_bytes={} owners={} ranges={} pending_release={}",
                      observed.domain, observed.total_bytes, observed.available_bytes, observed.owners, observed.ranges, observed.pending_release);
        });
    }

    /** Format an owned human-readable shared-byte-domain snapshot.
     * @return Allocation/range/available-byte text; free percentage is n/a with no recorded bytes.
     * Formatting/allocation errors propagate; this is not a residency limit or allocation request.
     */
    template <double gf_, int32_t gb_> std::string Manager<gf_, gb_>::stats() {
        const auto observed = diagnostics();
        const auto available = observed.available_bytes;
        const auto total = observed.total_bytes;
        const auto owners = observed.owners;
        const auto ranges = observed.ranges;
        // A free percentage is unavailable without a recorded allocation.
        const auto percentage =
            total == 0 ? std::string{"n/a (no allocations)"} : std::format("{:2.1f}%", (available / (double)total) * 100);
        auto tot = format_bytes(total);
        auto avail = format_bytes(available);
        return std::format(
            "Recorded stats regarding allocations from this manager\n"
            "heap allocations: {}\n"
            "heap ranges: {}\n"
            "not in use: {}\n"
            "total: {}\n"
            "available: {}\n\n",
            owners, ranges, percentage, tot, avail
        );
    }

    /** Copy reusable ranges under the pool read lock, then format outside it.
     * @return Owned range listing; no direct references to mutable collections escape.
     * Snapshot/formatting allocation failures propagate.
     */
    template <double gf_, int32_t gb_> std::string Manager<gf_, gb_>::debug_info() {
        std::vector<ByteBlock> snapshot;
        {
            std::shared_lock lock(get_mutex(pool));
            snapshot.assign(std::get<1>(this->pool).begin(), std::get<1>(this->pool).end());
        }
        std::stringstream ss;
        for (const auto& b : snapshot)
            ss << b << std::endl;
        return ss.str();
    }

    /** Return the complete checked-out section containing an address.
     * @param ptr Address in a known checked-out section/owner; this compatibility operation ignores an unknown or already-pooled result.
     * It returns the whole containing range, not just bytes after ptr. Stop all
     * users first; no T destruction, facade teardown or immediate native free occurs.
     */
    template <double gf_, int32_t gb_> void Manager<gf_, gb_>::return_ptr(void* ptr) {
        BlockTransactions<void>::return_range(this->state_, ptr, std::nullopt);
    }

    /** Return exactly a byte subrange beginning at ptr, preserving remaining checked-out sections.
     * @param ptr First byte of the returned range within a current checked-out section.
     * @param length Positive in-bounds byte count, not an object count.
     * @throws CE::Exceptions::bad_request For unknown, pooled or out-of-bounds ranges.
     * Stop all users of returned bytes before calling. Transaction allocation/locking
     * failures propagate without a partially committed logical partition.
     */
    template <double growth_factor_, int32_t growth_base_>
    void Manager<growth_factor_, growth_base_>::return_portion(void* ptr, std::size_t length) {
        if (!BlockTransactions<void>::return_range(this->state_, ptr, length)) {
            throw ::CE::Exceptions::bad_request(CE_HERE, "Cannot return an unknown, pooled, or out-of-bounds memory portion.");
        }
    }

    /** Return an exact current byte Block through the retained release context.
     * @param returned Known checked-out owner/head/length metadata; aliases must no longer use its bytes.
     * Invalid/double returns throw failed_operation; pooled backing remains retained
     * until culling releases complete owners. This does not construct/destroy objects.
     */
    template <double gf_, int32_t gb_> void Manager<gf_, gb_>::return_chunk(const ByteBlock& returned) {
        release_context_->return_chunk(returned);
    }

    /** Obtain a checked-out byte range by reusing shared pool storage or allocating backing outside locks.
     * @param length Positive minimum bytes before fit-policy growth.
     * @param alignment Requested byte alignment, rounded to a power of two of at least 64; zero selects that minimum.
     * @param fit exact retains length, larger adds growth_base, greedy truncates growth_factor*length then adds growth_base.
     * @param growth_base Added bytes for larger/greedy policies.
     * @param growth_factor Finite positive greedy multiplier with representable nonshrinking result.
     * @return Owned byte-range metadata, possibly larger due to aligned tail partitioning; storage remains unconstructed.
     * Invalid/overflowing requests throw bad_request; allocation failures propagate.
     * Bookkeeping commits transactionally, but user access to checked-out bytes is unsynchronized.
     */
    template <double gf_, int32_t gb_>
    ByteBlock Manager<gf_, gb_>::checkout_chunk(size_t length, size_t alignment, FitPolicy fit, size_t growth_base, double growth_factor) {
        if (length == 0) {
            throw ::CE::Exceptions::bad_request(CE_HERE, "Cannot check out an empty memory block.");
        }
        const auto request_length = adjust_length(length, fit, growth_base, growth_factor);
        if (request_length < length || alignment > (std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1))) {
            throw ::CE::Exceptions::bad_request(CE_HERE, "The requested memory length or alignment cannot be represented.");
        }
        const auto requested_alignment = std::bit_ceil(std::max(alignment, std::size_t{64}));
        const auto align_val = static_cast<std::align_val_t>(requested_alignment);
        return BlockTransactions<void>::checkout(this->state_, request_length, align_val, [=] {
            return allocate(request_length, align_val);
        });
    }

    /** Add reusable backing allocations without checking them out to a caller.
     * @param blocks Number of independent allocations; zero adds none after validating width/alignment.
     * @param width Positive base width in bytes.
     * @param gb Added bytes after greedy growth, independent of the number of blocks.
     * @param gf Finite positive multiplier yielding representable length at least width.
     * @param alignment Nonzero representable byte alignment, rounded upward to a power of two.
     * Invalid growth/size/alignment throws bad_request. A later allocation failure
     * leaves earlier complete owners registered and eligible for ordinary culling;
     * there is no multi-block rollback. Reporting occurs after transaction locks release.
     */
    template <double gf_, int32_t gb_>
    void Manager<gf_, gb_>::preallocate(size_t blocks, size_t width, size_t gb, double gf, std::align_val_t alignment) {
        const auto requested_alignment = static_cast<std::size_t>(alignment);
        if (width == 0 || requested_alignment == 0 ||
            requested_alignment > (std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1))) {
            throw ::CE::Exceptions::bad_request(CE_HERE, "Preallocation requires a nonempty length and representable alignment.");
        }
        const auto len = adjust_length(width, greedy, gb, gf);
        if (len < width)
            throw ::CE::Exceptions::bad_request(CE_HERE, "Preallocation growth cannot reduce the requested width.");
        // Each owner is a complete transaction; a failed later allocation leaves
        // earlier preallocations available and eligible for normal culling.
        std::size_t completed = 0;
        try {
            for (; completed < blocks; ++completed) {
                const auto block = allocate(len, alignment);
                BlockTransactions<void>::register_free(this->state_, block);
            }
        } catch (...) {
            CE_LOG_ERROR(
                ::CE::Logging::Categories::memory,
                "subsystem=memory domain={} operation=preallocate outcome=partial completed={} requested={} bytes_per_block={}",
                this->state_->domain, completed, blocks, len
            );
            throw;
        }
        CE_LOG_INFO(
            ::CE::Logging::Categories::memory,
            "subsystem=memory domain={} operation=preallocate outcome=completed blocks={} bytes_per_block={}", this->state_->domain,
            completed, len
        );
    }
}
