#pragma once
#ifndef MEMMGRHPP
#define MEMMGRHPP

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
        if (auto sec = find_section(ptr); sec.has_value() && sec->contains(ptr)) {
            return_chunk(*sec);
        } else if (auto owner = find_owner(ptr); owner.has_value() && owner->contains(ptr)) {
            return_chunk(*owner);
        }
    }

    template<double growth_factor_, int32_t growth_base_>
    void Manager<growth_factor_, growth_base_>::return_portion(void* ptr, std::size_t length) {
        MFATAL() << "I thought this wasn't being used in this binary/test";
        auto ret_chunk = [](OBlock b, void* ptr, std::size_t length) {
            const auto original = *b;
            auto block_end = ptr::offset_address(b->head.get(), b->length);
            auto end = ptr::offset_address(ptr, length);
            if (end <= block_end) {
                bool sec_changed = false;
                auto remainder_end = block_end - end;
                auto remainder_front = reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(b->head.get());
                if (remainder_front > 0) {
                    sec_changed = true;
                    auto back_end = b->split_exactly(remainder_front);
                    emplace(*b, sections);
                    b = back_end;
                }
                if (remainder_end > 0) {
                    sec_changed = true;
                    auto back_end = b->split_exactly(remainder_front);
                    emplace(*back_end, sections);
                }
                if (sec_changed) {
                    erase(original, sections);
                    emplace(*b, sections);
                }
            } else {
                CELog::warn("Mem::Manager::return_portion: Invalid portion returned. The length of the portion is too large from the location of the pointer returned.");
            }
            Manager::get().return_chunk(*b);
        };
        if (auto sec = find_section(ptr); sec.has_value() && sec->contains(ptr)) {
            ret_chunk(sec, ptr, length);
        } else if (auto owner = find_owner(ptr); owner.has_value() && owner->contains(ptr)) {
            ret_chunk(owner, ptr, length);
        }
    }

    template<double gf_, int32_t gb_>
    void Manager<gf_,gb_>::return_chunk(const Block &returned) {
        if (!contains(returned, sections) && !contains(returned, registry)) {
            CELog::critical("Cannot return Block. No such block exists. Block: {}", returned);
            MTRACE() << debug_info();
            throw Exceptions::failed_operation(CE_HERE,"Memory Manager was returned an unknown block");
        }
        merge_into_pool(returned);
    }

    template<double gf_, int32_t gb_>
    Block Manager<gf_,gb_>::checkout_chunk(size_t length, size_t alignment, Enum::fitType fit, size_t growth_base, double growth_factor) {
        const auto request_length = Math::adjust_length(length, fit, growth_base, growth_factor);
        const auto align_val = static_cast<std::align_val_t>(alignment);
        OBlock ob = fill_request(request_length);
        const bool request_filled = ob.has_value();
        // do we have an allocation?
        if (!request_filled) {
            // no? we'll make an allocation now
            ob = allocate(request_length, align_val);
            MTRACE() << "allocated: " << *ob;
            record_new(*ob);
        }
        // we will now know precisely how much we're passing along, it may be up to 63 bytes extra
        if (request_filled && contains(*ob, registry)) {
            // an existing block may still be marked stale
            erase(*ob, stale, release);
        }
        auto original = *ob;
        const auto right = ob->split_exactly(request_length);
        // we only need records if the right portion exists, because sections only deals in sub-blocks
        if (right.has_value()) {
            erase(original, sections);

            // record the left portion
            emplace(*ob, sections);
            // record the right portion
            emplace(*right, sections);
            merge_into_pool(*right);
            MTRACE() << "taking " << *right << " back to the pool.";
        }
        MTRACE() << "giving " << *ob << " to caller.";
        return *ob;
    }

    template<double gf_, int32_t gb_>
    void Manager<gf_,gb_>::preallocate(size_t blocks, size_t width, size_t gb, double gf, std::align_val_t alignment) {
        const auto len = Math::adjust_length(width, Enum::greedy, gb_, gf_);
        MINFO() << "Pre-allocating " << blocks << " " << width << " byte wide blocks aligned to " << alignment;
        for(int i = 0; i < blocks; ++i) {
            const auto a = allocate(len, alignment);
            MTRACE() << "preallocated: " << a;
            emplace(a, registry, pool);
            mark_stale(a);
        }
    }
}

#endif
