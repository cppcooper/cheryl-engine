#pragma once

/** @file
 * @brief Engine exception categories and owned diagnostic messages.
 * This installed header keeps its historical internals/ include path. The
 * exception types form the public failure contract of Engine operations.
 * @see [Failure reporting](cheryl-guide:runtime/failure-reporting)
 */

#include <cheryl/export.h>
#include "macros/debug.h"
#include <exception>
#include <stdexcept>
#include <format>
#include <string>
#include <iostream>
#include <cinttypes>
#include <array>
#include <charconv>
#include <utility>

namespace CE::Exceptions {
    /** Owned diagnostics on every compiler. Construction helpers catch trace/format
     * allocation failure and store bounded category/location/info without allocation.
     * The fallback changes the exception layout; consumers must rebuild together.
     * Allocation in caller argument expressions remains the caller's responsibility.
     * what()/diagnostic_summary() borrow this exception's owned storage; retain the
     * exception and exclude concurrent mutation while inspecting those pointers.
     * @see [Failure reporting](cheryl-guide:runtime/failure-reporting)
     */
    class CE_TYPE_API exception_base : public std::exception {
        std::string msg;
        std::array<char, 512> fallback_{};

        exception_base() noexcept = default;
        void append_fallback(const char* text, std::size_t& used) noexcept {
            if (!text)
                text = "(unknown)";
            for (std::size_t index = 0; text[index] && used < fallback_.size() - 1; ++index)
                fallback_[used++] = text[index];
            fallback_[used] = '\0';
        }

    public:
        explicit exception_base(std::string msg) noexcept
        : msg(std::move(msg)) {}
        exception_base(std::string msg, exception_base fallback) noexcept
        : msg(std::move(msg)), fallback_(fallback.fallback_) {}
        exception_base(const exception_base& other) noexcept
        : fallback_(other.fallback_) {
            try {
                msg = other.msg;
            } catch (...) {
                msg.clear();
                if (!fallback_[0]) {
                    std::size_t used = 0;
                    append_fallback(other.what(), used);
                }
            }
        }
        exception_base(exception_base&& other) noexcept = default;
        CE_API ~exception_base() override;
        [[nodiscard]] const char* what() const noexcept override { return msg.empty() ? fallback_.data() : msg.c_str(); }
        // Emergency output prefers cause/location over the allocating trace prefix.
        [[nodiscard]] const char* diagnostic_summary() const noexcept { return fallback_[0] ? fallback_.data() : what(); }

        [[nodiscard]] static exception_base fallback(
            const char* category,
            const char* location,
            uint32_t line,
            const char* info
        ) noexcept {
            exception_base result;
            std::size_t used = 0;
            result.append_fallback("exception: ", used);
            result.append_fallback(category, used);
            result.append_fallback(" at line ", used);
            std::array<char, 11> digits{};
            const auto number = std::to_chars(digits.data(), digits.data() + digits.size() - 1, line);
            *number.ptr = '\0';
            result.append_fallback(digits.data(), used);
            result.append_fallback(" inside ", used);
            result.append_fallback(location, used);
            result.append_fallback("\n", used);
            result.append_fallback(info ? info : "", used);
            return result;
        }
    };

    /** An operation received invalid configuration, values or argument combinations. This is an Engine category, not std::invalid_argument; each operation documents its rejected inputs.
     * Constructors retain diagnostic text without throwing; their caller arguments
     * must still be evaluated before entry.
     */
    class CE_TYPE_API invalid_args : public exception_base {
    public:
        CE_API invalid_args(const char* location_, uint32_t line_) noexcept;
        CE_API invalid_args(const char* location_, uint32_t line_, const char* info_) noexcept;
        invalid_args(const char* location_, uint32_t line_, const std::string& info_) noexcept
        : invalid_args(location_, line_, info_.c_str()) {}
        invalid_args(const invalid_args&) noexcept = default;
        invalid_args(invalid_args&&) noexcept = default;
        CE_API ~invalid_args() override; ///< Engine owns destruction and diagnostic RTTI; copies/moves retain existing semantics.
    };

    /** Base category for an Engine operation that fails during execution, including the more specific bad_alloc, bad_request and failed_operation categories.
     * Constructors retain diagnostic text without throwing; their caller arguments
     * must still be evaluated before entry.
     */
    class CE_TYPE_API runtime_exception : public exception_base {
    public:
        CE_API runtime_exception(const char* location_, uint32_t line_, const char* info_) noexcept;
        runtime_exception(const char* location_, uint32_t line_, const std::string& info_) noexcept
        : runtime_exception(location_, line_, info_.c_str()) {}
        CE_API runtime_exception(const char* sub_type, const char* location_, uint32_t line_, const char* info_) noexcept;
    };

    /** Engine backing-allocation failure category. This derives from runtime_exception, not std::bad_alloc; an operation may also propagate standard allocation exceptions.
     * Constructors retain diagnostic text without throwing; their caller arguments
     * must still be evaluated before entry.
     */
    class CE_TYPE_API bad_alloc : public runtime_exception {
    public:
        CE_API bad_alloc(const char* location_, uint32_t line_) noexcept;
    };

    /** A request violates the receiving operation's state, range or ownership rules. Consult that operation for valid inputs and preserved state.
     * Constructors retain diagnostic text without throwing; their caller arguments
     * must still be evaluated before entry.
     */
    class CE_TYPE_API bad_request : public runtime_exception {
    public:
        CE_API bad_request(const char* location_, uint32_t line_, const char* info_) noexcept;
        bad_request(const char* location_, uint32_t line_, const std::string& info_) noexcept
        : bad_request(location_, line_, info_.c_str()) {}
        bad_request(const bad_request&) noexcept = default;
        bad_request(bad_request&&) noexcept = default;
        CE_API ~bad_request() override; ///< Engine owns destruction and diagnostic RTTI; copies/moves retain existing semantics.
    };

    /** An operation could not complete. The reporting operation defines whether earlier effects remain; this category alone promises no rollback.
     * Constructors retain diagnostic text without throwing; their caller arguments
     * must still be evaluated before entry.
     */
    class CE_TYPE_API failed_operation : public runtime_exception {
    public:
        CE_API failed_operation(const char* location_, uint32_t line_, const char* info_) noexcept;
        failed_operation(const char* location_, uint32_t line_, const std::string& info_) noexcept
        : failed_operation(location_, line_, info_.c_str()) {}
    };
}

/**
 * @fn CE::Exceptions::exception_base::exception_base(std::string msg) noexcept
 * @brief Take ownership of an already-created message without formatting or a trace.
 * @param msg Owned message; empty yields an empty what() string.
 * String creation before entry may throw; moving it into this exception does not allocate.
 */

/**
 * @fn CE::Exceptions::exception_base::exception_base(std::string msg, exception_base fallback) noexcept
 * @brief Own a full message and copy the supplied bounded emergency summary.
 * @param msg Full message, normally including an Engine trace/category/location.
 * @param fallback Exception carrying fixed-buffer fallback text.
 * what() prefers a nonempty full message; diagnostic_summary() prefers the copied fallback. Call-site argument evaluation can allocate before entry.
 */

/**
 * @fn CE::Exceptions::exception_base::exception_base(const exception_base& other) noexcept
 * @brief Copy owned diagnostic text, retaining a bounded message if allocation fails.
 * @param other Stable source exception.
 * On message-copy failure, keep the source fallback or copy/truncate other.what() into the fixed buffer. Copying does not propagate allocation failure.
 */

/**
 * @fn CE::Exceptions::exception_base::exception_base(exception_base&& other) noexcept
 * @brief Move the owned message and copy the bounded summary.
 * @param other Source exception; its subsequent what() remains valid but its full message content is unspecified.
 */

/**
 * @fn CE::Exceptions::exception_base::~exception_base()
 * @brief Release owned text through a virtual, nonthrowing base destructor.
 */

/**
 * @fn const char* CE::Exceptions::exception_base::what() const noexcept
 * @return Borrowed NUL-terminated full message, or the bounded fallback when the full string is empty. Valid until this exception is mutated or destroyed; no allocation or logging occurs.
 */

/**
 * @fn const char* CE::Exceptions::exception_base::diagnostic_summary() const noexcept
 * @return Borrowed bounded category/location/info when present, otherwise what(). Emergency reporting uses this to prioritize the cause over an allocating trace prefix; no formatting, allocation or output occurs.
 */

/**
 * @fn CE::Exceptions::exception_base CE::Exceptions::exception_base::fallback(const char* category, const char* location, uint32_t line, const char* info) noexcept
 * @brief Build bounded owned diagnostics without allocation or stack capture.
 * @param category NUL-terminated category, or null for an unknown marker.
 * @param location NUL-terminated source location, or null for an unknown marker.
 * @param line Source line number, formatted as unsigned decimal.
 * @param info Optional NUL-terminated detail; null appends no detail.
 * @return Owned exception with at most 511 bytes plus a terminating NUL; later text is truncated when the fixed buffer fills. Pointers are read during the call, not retained. No logging or native callback is invoked.
 */

/**
 * @fn CE::Exceptions::invalid_args::invalid_args(const char* location_, uint32_t line_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */

/**
 * @fn CE::Exceptions::invalid_args::invalid_args(const char* location_, uint32_t line_, const char* info_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * @param info_ Detail copied during construction; null C strings append no detail. Embedded NUL in the string overload ends the copied detail.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */

/**
 * @fn CE::Exceptions::invalid_args::invalid_args(const char* location_, uint32_t line_, const std::string& info_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * @param info_ Detail copied during construction; null C strings append no detail. Embedded NUL in the string overload ends the copied detail.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */

/**
 * @fn CE::Exceptions::runtime_exception::runtime_exception(const char* location_, uint32_t line_, const char* info_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * @param info_ Detail copied during construction; null C strings append no detail. Embedded NUL in the string overload ends the copied detail.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */

/**
 * @fn CE::Exceptions::runtime_exception::runtime_exception(const char* location_, uint32_t line_, const std::string& info_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * @param info_ Detail copied during construction; null C strings append no detail. Embedded NUL in the string overload ends the copied detail.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */

/**
 * @fn CE::Exceptions::runtime_exception::runtime_exception(const char* sub_type, const char* location_, uint32_t line_, const char* info_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * @param info_ Detail copied during construction; null C strings append no detail. Embedded NUL in the string overload ends the copied detail.
 * @param sub_type Runtime subcategory; null uses the generic runtime category.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */

/**
 * @fn CE::Exceptions::bad_alloc::bad_alloc(const char* location_, uint32_t line_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */

/**
 * @fn CE::Exceptions::bad_request::bad_request(const char* location_, uint32_t line_, const char* info_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * @param info_ Detail copied during construction; null C strings append no detail. Embedded NUL in the string overload ends the copied detail.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */

/**
 * @fn CE::Exceptions::bad_request::bad_request(const char* location_, uint32_t line_, const std::string& info_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * @param info_ Detail copied during construction; null C strings append no detail. Embedded NUL in the string overload ends the copied detail.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */

/**
 * @fn CE::Exceptions::failed_operation::failed_operation(const char* location_, uint32_t line_, const char* info_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * @param info_ Detail copied during construction; null C strings append no detail. Embedded NUL in the string overload ends the copied detail.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */

/**
 * @fn CE::Exceptions::failed_operation::failed_operation(const char* location_, uint32_t line_, const std::string& info_) noexcept
 * @brief Capture owned category/location diagnostics, with a trace when capture/formatting succeeds.
 * @param location_ NUL-terminated source location, normally supplied with the line by CE_HERE; null uses an unknown marker.
 * @param line_ Source line number.
 * @param info_ Detail copied during construction; null C strings append no detail. Embedded NUL in the string overload ends the copied detail.
 * Trace/format/allocation failures inside the constructor use the bounded emergency summary instead of escaping. Input pointers are not retained. This does not log, throw itself, or roll back the failed operation; the caller decides whether to throw the constructed object.
 */
