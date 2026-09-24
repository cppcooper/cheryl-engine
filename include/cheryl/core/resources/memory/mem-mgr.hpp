#pragma once
#include "mem-mgr.h"
#include <internals/celog.h>
#include <math/fit.h>
#include <math/bytes.h>
#include <optional>
#include <utility>
#include <format>

namespace CE::Mem {
    template<double gf_, int32_t gb_>
    std::string Manager<gf_,gb_>::stats() {
        // Read the reusable and registered ranges under shared locks for one bookkeeping snapshot.
        std::shared_lock l1(get_mutex(pool));
        std::shared_lock l2(get_mutex(sections));
        std::shared_lock l3(get_mutex(registry));
        size_t available = 0;
        size_t total = 0;
        for(const auto& b : std::get<1>(this->pool)) {
            available += b.length;
        }
        for(const auto &b : std::get<1>(this->registry)) {
            total += b.length;
        }
        // TODO: Define a zero-allocation result before dividing by total; an untouched
        // manager currently formats 0/0 as the free percentage.
        auto tot = human_readable(total);
        auto avail = human_readable(available);
        return std::format(
            "Recorded stats regarding allocations from this manager\n"
            "heap allocations: {}\n"
            "heap ranges: {}\n"
            "not in use: {:2.1f}%\n"
            "total: {}\n"
            "available: {}\n\n",
            std::get<1>(this->registry).size(),
            std::get<1>(this->sections).size(),
            (available/(double)total)*100,
            tot, avail);
    }

    template<double gf_, int32_t gb_>
    std::string Manager<gf_,gb_>::debug_info() {
        std::stringstream ss;
        std::shared_lock l1(get_mutex(pool));
        for(const auto& b : std::get<1>(this->pool)) {
            ss << b << std::endl;
        }
        return ss.str();
    }

    template<double gf_, int32_t gb_>
    void Manager<gf_,gb_>::return_ptr(void* ptr) {
        // Split owners are tracked by section; whole allocations may still
        // exist only in the registry. Give the narrower section first chance.
        if (auto sec = find_section(ptr); sec.has_value() && sec->contains(ptr)) {
            return_chunk(*sec);
        } else if (auto owner = find_owner(ptr); owner.has_value() && owner->contains(ptr)) {
            return_chunk(*owner);
        }
    }

    template<double growth_factor_, int32_t growth_base_>
    void Manager<growth_factor_, growth_base_>::return_portion(void* ptr, std::size_t length) {
        // Resolve the active subrange (or whole owner) and reject a return that crosses its end.
        const auto section = find_section(ptr);
        const auto block = section.has_value() ? section : find_owner(ptr);
        if (!block.has_value() || length == 0 || !block->contains(ptr) || contains(*block, pool)) {
            throw Exceptions::bad_request(CE_HERE, "Cannot return an unknown or already pooled memory portion.");
        }
        const auto offset = reinterpret_cast<std::uintptr_t>(ptr) -
                            reinterpret_cast<std::uintptr_t>(block->head.get());
        if (length > block->length - offset) {
            throw Exceptions::bad_request(CE_HERE, "The returned memory portion exceeds its active block.");
        }
        if (offset == 0 && length == block->length) {
            // An exact return follows the whole-block path, including its
            // active-section check and eventual stale-owner bookkeeping.
            return_chunk(*block);
            return;
        }

        // Partition around the returned slice. Keep unreleased pieces in sections and merge
        // only the returned slice with adjacent reusable ranges in the pool.
        erase(*block, sections);
        auto returned = *block;
        if (offset > 0) {
            auto remaining = returned.split_exactly(offset);
            emplace(returned, sections);
            returned = *remaining;
        }
        if (length < returned.length) {
            auto remaining = returned.split_exactly(length);
            emplace(*remaining, sections);
        }
        emplace(returned, sections);
        merge_into_pool(returned);
    }

    template<double gf_, int32_t gb_>
    void Manager<gf_,gb_>::return_chunk(const Block &returned) {
        // A whole owner cannot be returned while any of its split sections remain in use.
        const bool in_sections = contains(returned, sections);
        const bool in_registry = contains(returned, registry);
        bool has_active_sections = false;
        if (in_registry) {
            std::shared_lock lock(std::get<0>(sections));
            for (const auto& section : std::get<1>(sections)) {
                if (section.owner.get() == returned.owner.get()) {
                    has_active_sections = true;
                    break;
                }
            }
        }
        if ((!in_sections && !in_registry) || contains(returned, pool) || has_active_sections) {
            CELog::critical("Cannot return Block. No such block exists. Block: {}", returned);
            MTRACE() << debug_info();
            throw Exceptions::failed_operation(CE_HERE,"Memory Manager was returned an unknown block");
        }
        // merge_into_pool owns the free-range transition: adjacent free
        // sections coalesce and a complete owner becomes eligible for culling.
        merge_into_pool(returned);
    }

    template<double gf_, int32_t gb_>
    Block Manager<gf_,gb_>::checkout_chunk(size_t length, size_t alignment, Enum::fitType fit, size_t growth_base, double growth_factor) {
        if (length == 0) {
            throw Exceptions::bad_request(CE_HERE, "Cannot check out an empty memory block.");
        }
        const auto request_length = Math::adjust_length(length, fit, growth_base, growth_factor);
        const auto requested_alignment = std::bit_ceil(std::max(alignment, std::size_t{64}));
        const auto align_val = static_cast<std::align_val_t>(requested_alignment);
        // Reuse a suitable pool range or allocate/record a new backing owner.
        OBlock ob = fill_request(request_length, align_val);
        const bool request_filled = ob.has_value();
        if (!request_filled) {
            ob = allocate(request_length, align_val);
            MTRACE() << "allocated: " << *ob;
            record_new(*ob);
        }
        // Reclaiming a stale whole owner cancels its pending cull before checkout.
        if (request_filled && contains(*ob, registry)) {
            // A previously free owner may still be queued for deferred release.
            erase(*ob, stale, release);
        }
        auto original = *ob;
        const auto right = ob->split_at(request_length);
        // A split makes both portions sections; immediately return the aligned spare tail to
        // the pool. An unsplit range remains represented by its original owner record.
        if (right.has_value()) {
            erase(original, sections);

            // Both halves replace the former section record; the requested
            // head stays checked out while only the spare tail becomes free.
            emplace(*ob, sections);
            emplace(*right, sections);
            merge_into_pool(*right);
            MTRACE() << "taking " << *right << " back to the pool.";
        }
        MTRACE() << "giving " << *ob << " to caller.";
        return *ob;
    }

    template<double gf_, int32_t gb_>
    void Manager<gf_,gb_>::preallocate(size_t blocks, size_t width, size_t gb, double gf, std::align_val_t alignment) {
        // Seed the registry and reusable pool together, then start the stale
        // clock for each untouched allocation so normal culling can reclaim it.
        const auto len = Math::adjust_length(width, Enum::greedy, gb, gf);
        MINFO() << "Pre-allocating " << blocks << " " << width << " byte wide blocks aligned to " << alignment;
        for(int i = 0; i < blocks; ++i) {
            const auto a = allocate(len, alignment);
            MTRACE() << "preallocated: " << a;
            emplace(a, registry, pool);
            mark_stale(a);
        }
    }
}
