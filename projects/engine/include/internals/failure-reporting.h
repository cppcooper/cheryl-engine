#pragma once
#include "exceptions.h"
#include <core/diagnostics.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <utility>

namespace CE::Diagnostics {
    // Bounded operational context for required/noexcept failure paths. The
    // strings are engine-owned labels, not payloads, paths, or user text.
    inline void report_outcome(
        const char* subsystem,
        const DomainId domain,
        const char* operation,
        const char* outcome,
        const std::uint64_t count = 0
    ) noexcept {
        std::array<char, 384> record{};
        const auto length = std::snprintf(
            record.data(), record.size(), "[Cheryl diagnostic] subsystem=%s domain=%llu operation=%s outcome=%s count=%llu\n",
            subsystem, static_cast<unsigned long long>(domain), operation, outcome, static_cast<unsigned long long>(count)
        );
        if (length > 0)
            (void)std::fwrite(record.data(), 1, std::min(static_cast<std::size_t>(length), record.size() - 1), stderr);
    }

    /**
     * Best-effort emergency reporting, independent of the logger and symbolizer.
     * A bounded record is written with C stdio; no C++ allocation, formatting,
     * callbacks, or exceptions are required. Truncation and output errors do not
     * replace the failure being handled. Call outside application/collection locks.
     * The optional destination is borrowed and must remain open for this call.
     */
    inline void report_failure(const char* phase, std::exception_ptr failure, std::FILE* destination = stderr) noexcept {
        std::array<char, 1024> record{};
        std::size_t used = 0;
        const auto append = [&](const char* text) {
            if (!text)
                text = "(unknown)";
            for (std::size_t index = 0; text[index] && used < record.size() - 1; ++index)
                record[used++] = text[index];
        };
        append("[Cheryl failure] ");
        append(phase);
        append(": ");
        try {
            if (failure)
                std::rethrow_exception(failure);
            append("no exception information");
        } catch (const Exceptions::exception_base& error) {
            append(error.diagnostic_summary());
        } catch (const std::exception& error) {
            append(error.what());
        } catch (...) {
            append("non-standard exception");
        }
        record[used++] = '\n';
        if (destination) {
            (void)std::fwrite(record.data(), 1, used, destination);
            (void)std::fflush(destination);
        }
    }

    /** Keep the first exception; report distinct later failures with their phase. */
    inline void preserve_failure(std::exception_ptr& first, const char* phase, std::exception_ptr next) noexcept {
        if (!next)
            return;
        if (!first)
            first = std::move(next);
        else if (first != next)
            report_failure(phase, std::move(next));
    }
}
