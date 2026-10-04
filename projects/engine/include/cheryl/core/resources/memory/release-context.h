#pragma once
#include "typedefs.h"
#include <internals/celog.h>
#include <internals/compile-time-logging.hpp>
#include <internals/failure-reporting.h>

#include <exception>

namespace CE::Mem {
    /** Retains byte bookkeeping for final releases independently of a Manager facade.
     * All void managers share this domain; growth policy only affects checkout.
     * Handles capture this context before the facade can be destroyed. Returning
     * storage never dereferences that facade or performs a singleton lookup.
     * Returns validate and commit under the shared domain's transaction locks.
     */
    struct ByteReleaseContext final : AbstractManager<void> {
        void return_chunk(const HeapBlock& returned) {
            if (!BlockTransactions<void>::return_block(this->state_, returned)) {
                // Reporting is outside the bookkeeping locks.
                CE_LOG_CRITICAL(CE::memlog, "Cannot return Block. No such block exists. Block: {}", returned);
                throw Exceptions::failed_operation(CE_HERE, "Memory Manager was returned an unknown block");
            }
        }

        // Internal final owners return a known live range. Invalid bookkeeping or
        // allocation failure terminates instead of escaping a shared_ptr deleter.
        void release_owned(const HeapBlock& returned) noexcept {
            try {
                // Final release must not initialize/use the ordinary logger during
                // static teardown, including the invalid-bookkeeping path.
                if (!BlockTransactions<void>::return_block(this->state_, returned)) {
                    throw Exceptions::failed_operation(CE_HERE, "Owned byte release found invalid bookkeeping");
                }
            } catch (...) {
                Diagnostics::report_failure("owned byte release", std::current_exception());
                std::terminate();
            }
        }
    };
}
