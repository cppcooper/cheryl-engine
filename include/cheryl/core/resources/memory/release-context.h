#pragma once
#include "typedefs.h"
#include <internals/celog.h>

#include <exception>

namespace CE::Mem {
    /** Retains byte bookkeeping for final releases independently of a Manager facade.
     * All void managers share this domain; growth policy only affects checkout.
     * Handles capture this context before the facade can be destroyed. Returning
     * storage never dereferences that facade or performs a singleton lookup.
     * Cross-container transaction synchronization remains the manager audit's concern.
     */
    struct ByteReleaseContext final : AbstractManager<void> {
        void return_chunk(const HeapBlock& returned) {
            // A whole owner cannot be returned while any split sections remain active.
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
                throw Exceptions::failed_operation(CE_HERE, "Memory Manager was returned an unknown block");
            }
            merge_into_pool(returned);
        }

        // Internal final owners return a known live range. Invalid bookkeeping or
        // allocation failure terminates instead of escaping a shared_ptr deleter.
        void release_owned(const HeapBlock& returned) noexcept {
            try {
                return_chunk(returned);
            } catch (...) {
                std::terminate();
            }
        }
    };
}
