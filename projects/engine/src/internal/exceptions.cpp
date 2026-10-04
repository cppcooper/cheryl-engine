#include <internals/exceptions.h>
#include <internals/stack-trace-internal.h>
#include <format>

namespace {
    CE::Exceptions::exception_base make_exception(
        const char* category,
        const char* location,
        uint32_t line,
        const char* info = nullptr,
        bool runtime_subtype = false
    ) noexcept {
        auto fallback = CE::Exceptions::exception_base::fallback(category, location, line, info);
        try {
            // Trace evaluation belongs inside this guard, not among arguments
            // evaluated before entering a noexcept formatting helper.
            const auto trace = CE::TraceDetail::capture<2048>(nullptr, 12, 4);
            return CE::Exceptions::exception_base(
                std::format(
                    "{}\nexception: {}{} at line {} inside {}\n{}\n", trace, runtime_subtype ? "runtime exception: " : "", category,
                    line, location ? location : "(unknown)", info ? info : ""
                ),
                std::move(fallback)
            );
        } catch (...) {
            return fallback;
        }
    }
}

CE::Exceptions::invalid_args::invalid_args(const char* location_, uint32_t line_) noexcept
: CE::Exceptions::exception_base(make_exception("invalid args", location_, line_)) {}

CE::Exceptions::invalid_args::invalid_args(const char* location_, uint32_t line_, const char* info_) noexcept
: CE::Exceptions::exception_base(make_exception("invalid args", location_, line_, info_)) {}

CE::Exceptions::runtime_exception::runtime_exception(const char* location_, uint32_t line_, const char* info_) noexcept
: CE::Exceptions::exception_base(make_exception("runtime exception", location_, line_, info_)) {}

CE::Exceptions::runtime_exception::runtime_exception(
    const char* sub_type,
    const char* location_,
    uint32_t line_,
    const char* info_
) noexcept
: CE::Exceptions::exception_base(make_exception(sub_type ? sub_type : "runtime exception", location_, line_, info_, true)) {}

CE::Exceptions::bad_alloc::bad_alloc(const char* location_, uint32_t line_) noexcept
: CE::Exceptions::runtime_exception("bad_alloc", location_, line_, "heap allocation failed") {}

CE::Exceptions::bad_request::bad_request(const char* location_, uint32_t line_, const char* info_) noexcept
: CE::Exceptions::runtime_exception("bad_request", location_, line_, info_) {}

CE::Exceptions::failed_operation::failed_operation(const char* location_, uint32_t line_, const char* info_) noexcept
: CE::Exceptions::runtime_exception("failed_operation", location_, line_, info_) {}
