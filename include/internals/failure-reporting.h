#pragma once
#include <array>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <utility>

namespace CE::Diagnostics {
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
