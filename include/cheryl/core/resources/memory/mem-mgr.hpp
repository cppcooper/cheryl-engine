#pragma once
#include "mem-mgr.h"
#include <internals/celog.h>
#include <math/fit.h>
#include <math/bytes.h>
#include <optional>
#include <utility>
#include <format>

namespace CE::Mem {
    template <double gf_, int32_t gb_> MemoryStats Manager<gf_, gb_>::diagnostics() const {
        MemoryStats result{this->state_->domain};
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

    template <double gf_, int32_t gb_> void Manager<gf_, gb_>::report_diagnostics() const noexcept {
        CE::Logger<CE::memlog>::template write_lazy<ctlog::DEBUG_>([&](auto& log) {
            const auto observed = diagnostics();
            log.debug("subsystem=memory domain={} operation=summary total_bytes={} available_bytes={} owners={} ranges={} pending_release={}",
                      observed.domain, observed.total_bytes, observed.available_bytes, observed.owners, observed.ranges, observed.pending_release);
        });
    }

    template <double gf_, int32_t gb_> std::string Manager<gf_, gb_>::stats() {
        const auto observed = diagnostics();
        const auto available = observed.available_bytes;
        const auto total = observed.total_bytes;
        const auto owners = observed.owners;
        const auto ranges = observed.ranges;
        // A free percentage is unavailable without a recorded allocation.
        const auto percentage =
            total == 0 ? std::string{"n/a (no allocations)"} : std::format("{:2.1f}%", (available / (double)total) * 100);
        auto tot = human_readable(total);
        auto avail = human_readable(available);
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

    template <double gf_, int32_t gb_> std::string Manager<gf_, gb_>::debug_info() {
        std::vector<Block> snapshot;
        {
            std::shared_lock lock(get_mutex(pool));
            snapshot.assign(std::get<1>(this->pool).begin(), std::get<1>(this->pool).end());
        }
        std::stringstream ss;
        for (const auto& b : snapshot)
            ss << b << std::endl;
        return ss.str();
    }

    template <double gf_, int32_t gb_> void Manager<gf_, gb_>::return_ptr(void* ptr) {
        BlockTransactions<void>::return_range(this->state_, ptr, std::nullopt);
    }

    template <double growth_factor_, int32_t growth_base_>
    void Manager<growth_factor_, growth_base_>::return_portion(void* ptr, std::size_t length) {
        if (!BlockTransactions<void>::return_range(this->state_, ptr, length)) {
            throw Exceptions::bad_request(CE_HERE, "Cannot return an unknown, pooled, or out-of-bounds memory portion.");
        }
    }

    template <double gf_, int32_t gb_> void Manager<gf_, gb_>::return_chunk(const Block& returned) {
        release_context_->return_chunk(returned);
    }

    template <double gf_, int32_t gb_>
    Block Manager<gf_, gb_>::checkout_chunk(size_t length, size_t alignment, Enum::fitType fit, size_t growth_base, double growth_factor) {
        if (length == 0) {
            throw Exceptions::bad_request(CE_HERE, "Cannot check out an empty memory block.");
        }
        const auto request_length = Math::adjust_length(length, fit, growth_base, growth_factor);
        if (request_length < length || alignment > (std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1))) {
            throw Exceptions::bad_request(CE_HERE, "The requested memory length or alignment cannot be represented.");
        }
        const auto requested_alignment = std::bit_ceil(std::max(alignment, std::size_t{64}));
        const auto align_val = static_cast<std::align_val_t>(requested_alignment);
        return BlockTransactions<void>::checkout(this->state_, request_length, align_val, [=] {
            return allocate(request_length, align_val);
        });
    }

    template <double gf_, int32_t gb_>
    void Manager<gf_, gb_>::preallocate(size_t blocks, size_t width, size_t gb, double gf, std::align_val_t alignment) {
        const auto requested_alignment = static_cast<std::size_t>(alignment);
        if (width == 0 || requested_alignment == 0 ||
            requested_alignment > (std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1))) {
            throw Exceptions::bad_request(CE_HERE, "Preallocation requires a nonempty length and representable alignment.");
        }
        const auto len = Math::adjust_length(width, Enum::greedy, gb, gf);
        if (len < width)
            throw Exceptions::bad_request(CE_HERE, "Preallocation growth cannot reduce the requested width.");
        // Each owner is a complete transaction; a failed later allocation leaves
        // earlier preallocations available and eligible for normal culling.
        std::size_t completed = 0;
        try {
            for (; completed < blocks; ++completed) {
                const auto block = allocate(len, alignment);
                BlockTransactions<void>::register_free(this->state_, block);
            }
        } catch (...) {
            CE_LOG_ERROR(CE::memlog, "subsystem=memory domain={} operation=preallocate outcome=partial completed={} requested={} bytes_per_block={}",
                         this->state_->domain, completed, blocks, len);
            throw;
        }
        CE_LOG_INFO(CE::memlog, "subsystem=memory domain={} operation=preallocate outcome=completed blocks={} bytes_per_block={}",
                    this->state_->domain, completed, len);
    }
}
