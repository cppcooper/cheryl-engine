#pragma once
#include "mem-mgr.h"
#include <internals/celog.h>
#include <math/fit.h>
#include <math/bytes.h>
#include <optional>
#include <utility>
#include <format>

namespace CE::Mem {
    template <double gf_, int32_t gb_> std::string Manager<gf_, gb_>::stats() {
        size_t available = 0;
        size_t total = 0;
        size_t owners = 0;
        size_t ranges = 0;
        {
            std::shared_lock l1(get_mutex(pool), std::defer_lock);
            std::shared_lock l2(get_mutex(sections), std::defer_lock);
            std::shared_lock l3(get_mutex(registry), std::defer_lock);
            std::lock(l1, l2, l3);
            for (const auto& b : std::get<1>(this->pool))
                available += b.length;
            for (const auto& b : std::get<1>(this->registry))
                total += b.length;
            owners = std::get<1>(this->registry).size();
            ranges = std::get<1>(this->sections).size();
        }
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
        for (std::size_t i = 0; i < blocks; ++i) {
            const auto block = allocate(len, alignment);
            BlockTransactions<void>::register_free(this->state_, block);
        }
    }
}
