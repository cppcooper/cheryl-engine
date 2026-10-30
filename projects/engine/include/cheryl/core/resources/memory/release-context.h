#pragma once
#include "typedefs.h"
#include <internals/celog.h>
#include <internals/compile-time-logging.hpp>
#include <internals/failure-reporting.h>

#include <exception>

namespace CE::Memory {
    /** Retains byte bookkeeping for final releases independently of a Manager facade.
     * All void managers share this domain; growth policy only affects checkout.
     * Handles capture this context before the facade can be destroyed. Returning
     * storage never dereferences that facade or performs a singleton lookup.
     * Returns validate and commit under the shared domain's transaction locks.
     */
    struct ByteReleaseContext final : AbstractBlockManager<void> {
        void return_chunk(const ::CE::Memory::HeapBlock& returned) {
            if (!::CE::Memory::BlockTransactions<void>::return_block(this->state_, returned)) {
                // Reporting is outside the bookkeeping locks.
                CE_LOG_CRITICAL(::CE::Logging::Categories::memory, "Cannot return Block. No such block exists. Block: {}", returned);
                throw ::CE::Exceptions::failed_operation(CE_HERE, "Memory Manager was returned an unknown block");
            }
        }

        // Internal final owners return a known live range. Invalid bookkeeping or
        // allocation failure terminates instead of escaping a shared_ptr deleter.
        void release_owned(const ::CE::Memory::HeapBlock& returned) noexcept {
            try {
                // Final release must not initialize/use the ordinary logger during
                // static teardown, including the invalid-bookkeeping path.
                if (!::CE::Memory::BlockTransactions<void>::return_block(this->state_, returned)) {
                    throw ::CE::Exceptions::failed_operation(CE_HERE, "Owned byte release found invalid bookkeeping");
                }
            } catch (...) {
                ::CE::Diagnostics::report_failure("owned byte release", std::current_exception());
                std::terminate();
            }
        }
    };
}

/**
 * @fn void CE::Memory::ByteReleaseContext::return_chunk(const CE::Memory::HeapBlock& returned)
 * @brief Return a complete checked-out byte range under shared-domain transaction locks.
 * @param returned Exact current range, with valid backing owner/head/length metadata.
 * Unknown/already-pooled/invalid ranges throw failed_operation after unlocked diagnostics. No objects are destroyed by this byte operation; backing ownership remains pooled until culling releases it.
 */

/**
 * @fn void CE::Memory::ByteReleaseContext::release_owned(const CE::Memory::HeapBlock& returned) noexcept
 * @brief Final-owner return independent of a Manager facade or singleton lookup.
 * @param returned Known valid checked-out byte range, returned exactly once after all users stop.
 * Invalid bookkeeping or allocation/locking failure is reported and terminates instead of escaping a shared_ptr deleter. Never use this to hide recoverable arbitrary-pointer validation.
 * @see [CPU memory lifetime](cheryl-guide:resources/resource-lifetime)
 */
