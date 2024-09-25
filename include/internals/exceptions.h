#pragma once
#include "macros/debug.h"
#include <exception>
#include <stdexcept>
#include <format>
#include <string>
#include <iostream>
#include <cinttypes>

namespace CE::Exceptions {
// single threaded function to construct a c-string for exception messages. UB if used on multiple threads concurrently

    #ifdef _MSC_VER
    class exception_base : public std::exception {
    public:
        explicit exception_base(const std::string &msg) noexcept : std::exception(msg.c_str()) {}; // NOLINT(*-unnecessary-value-param)
        exception_base(const exception_base& other) noexcept = default;
        exception_base(exception_base&& other) noexcept : std::exception(other) {}
    };
    #else
    class exception_base : public std::exception {
        std::string msg;
    public:
        explicit exception_base(std::string msg) noexcept : msg(std::move(msg)) {}
        exception_base(exception_base&& other) noexcept : msg(std::move(other.msg)) {}
        [[nodiscard]] const char* what() const noexcept override {
            return msg.c_str();
        }
    };
    #endif

    class invalid_args : public exception_base {
    public:
        invalid_args(const char* location_, uint32_t line_) noexcept;
        invalid_args(const char* location_, uint32_t line_, const char* info_) noexcept;
    };

    class runtime_exception : public exception_base {
    public:
        runtime_exception(const char* location_, uint32_t line_, const char* info_) noexcept;
        runtime_exception(const char* sub_type, const char* location_, uint32_t line_, const char* info_) noexcept;
    };

    class bad_alloc : public runtime_exception {
    public:
        bad_alloc(const char* location_, uint32_t line_) noexcept;
    };

    class bad_request : public runtime_exception {
    public:
        bad_request(const char* location_, uint32_t line_, const char* info_) noexcept;
    };

    class failed_operation : public runtime_exception {
    public:
        failed_operation(const char* location_, uint32_t line_, const char* info_) noexcept;
    };
}
