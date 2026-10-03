#pragma once
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
     */
    class exception_base : public std::exception {
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
        [[nodiscard]] const char* what() const noexcept override { return msg.empty() ? fallback_.data() : msg.c_str(); }

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

    class invalid_args : public exception_base {
    public:
        invalid_args(const char* location_, uint32_t line_) noexcept;
        invalid_args(const char* location_, uint32_t line_, const char* info_) noexcept;
        invalid_args(const char* location_, uint32_t line_, const std::string& info_) noexcept
        : invalid_args(location_, line_, info_.c_str()) {}
    };

    class runtime_exception : public exception_base {
    public:
        runtime_exception(const char* location_, uint32_t line_, const char* info_) noexcept;
        runtime_exception(const char* location_, uint32_t line_, const std::string& info_) noexcept
        : runtime_exception(location_, line_, info_.c_str()) {}
        runtime_exception(const char* sub_type, const char* location_, uint32_t line_, const char* info_) noexcept;
    };

    class bad_alloc : public runtime_exception {
    public:
        bad_alloc(const char* location_, uint32_t line_) noexcept;
    };

    class bad_request : public runtime_exception {
    public:
        bad_request(const char* location_, uint32_t line_, const char* info_) noexcept;
        bad_request(const char* location_, uint32_t line_, const std::string& info_) noexcept
        : bad_request(location_, line_, info_.c_str()) {}
    };

    class failed_operation : public runtime_exception {
    public:
        failed_operation(const char* location_, uint32_t line_, const char* info_) noexcept;
        failed_operation(const char* location_, uint32_t line_, const std::string& info_) noexcept
        : failed_operation(location_, line_, info_.c_str()) {}
    };
}
