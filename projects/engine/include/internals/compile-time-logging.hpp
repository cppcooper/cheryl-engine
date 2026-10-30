#pragma once
#include "internal-logs.h"
#include <core/logging/logger.h>
#include <sstream>
#include <functional>
#include <exception>

// Configure the profile/mask target-wide (core/logging/compile-policy.h).
// Stream and guarded formatted macros filter before evaluating arguments.
// Direct trace/info/etc. function calls still evaluate their supplied arguments.
#define LOGLINESTREAM(log_level, logname)                                                                                                  \
    ::CE::Logging::Streams::LineStream([](const std::string& s) { ::CE::Logging::Logger<logname>::get().log_level("{}", s); })
#define CTLOG(ctl, log_level, logname)                                                                                                     \
    if constexpr (!::CE::Logging::Compile::enabled(ctl)) {                                                                                 \
    } else if (                                                                                                                            \
        auto* ce_log_instance_ = ::CE::Logging::Compile::acquire_logger<logname>(static_cast<::CE::Logging::Compile::LogLevel>(ctl));      \
        !ce_log_instance_                                                                                                                  \
    ) {                                                                                                                                    \
    } else                                                                                                                                 \
        ::CE::Logging::Streams::LineStream([ce_log_instance_](const std::string& s) { ce_log_instance_->log_level("{}", s); })

#define UTRACE(logname) CTLOG(::CE::Logging::Compile::TRACE_, trace, logname)
#define UDEBUG(logname) CTLOG(::CE::Logging::Compile::DEBUG_, debug, logname)
#define UINFO(logname) CTLOG(::CE::Logging::Compile::INFO_, info, logname)
#define UWARN(logname) CTLOG(::CE::Logging::Compile::WARNING_, warn, logname)
#define UERROR(logname) CTLOG(::CE::Logging::Compile::ERROR_, error, logname)
#define UFATAL(logname) CTLOG(::CE::Logging::Compile::FATAL_, critical, logname)

// The lambda keeps argument preparation inside the guarded emission boundary.
#define CE_LOG(ctl, log_level, logname, ...)                                                                                               \
    do {                                                                                                                                   \
        ::CE::Logging::Logger<logname>::template write_lazy<ctl>([&](auto& ce_log_instance_) {                                             \
            ce_log_instance_.log_level(__VA_ARGS__);                                                                                       \
        });                                                                                                                                \
    } while (false)

#define CE_LOG_TRACE(logname, ...) CE_LOG(::CE::Logging::Compile::TRACE_, trace, logname, __VA_ARGS__)
#define CE_LOG_DEBUG(logname, ...) CE_LOG(::CE::Logging::Compile::DEBUG_, debug, logname, __VA_ARGS__)
#define CE_LOG_INFO(logname, ...) CE_LOG(::CE::Logging::Compile::INFO_, info, logname, __VA_ARGS__)
#define CE_LOG_WARN(logname, ...) CE_LOG(::CE::Logging::Compile::WARNING_, warn, logname, __VA_ARGS__)
#define CE_LOG_ERROR(logname, ...) CE_LOG(::CE::Logging::Compile::ERROR_, error, logname, __VA_ARGS__)
#define CE_LOG_CRITICAL(logname, ...) CE_LOG(::CE::Logging::Compile::FATAL_, critical, logname, __VA_ARGS__)

namespace CE::Logging::Compile {
    template <const char* name> [[nodiscard]] ::CE::Logging::Log<name>* acquire_logger(const LogLevel level) noexcept {
        if (::CE::Logging::Detail::suppress_backend_emission())
            return nullptr;
        try {
            auto& log = ::CE::Logging::Logger<name>::get();
            return log.should_log(runtime_level(level)) ? &log : nullptr;
        } catch (...) {
            ::CE::Diagnostics::report_failure("stream logger acquisition", std::current_exception());
            return nullptr;
        }
    }

}

namespace CE::Logging::Streams {
    struct LineStream : std::stringstream {
    protected:
        std::function<void(const std::string&)> m_logFunc; // Logging function
        const int uncaught_on_entry_;

    public:
        ~LineStream() override {
            // Do not emit a partial message while a failed argument/inserter is
            // unwinding. Submission/string-copy failures cannot escape destruction.
            if (!m_logFunc || std::uncaught_exceptions() > uncaught_on_entry_)
                return;
            try {
                m_logFunc(str());
            } catch (...) {
                ::CE::Diagnostics::report_failure("stream log emission", std::current_exception());
            }
        }

        // Constructor takes a logging function to be called on destruction
        explicit LineStream(std::function<void(const std::string&)> logFunc)
        : m_logFunc(std::move(logFunc)), uncaught_on_entry_(std::uncaught_exceptions()) {}

        // Move constructor
        LineStream(LineStream&& other)
        : std::stringstream(std::move(other)), m_logFunc(std::move(other.m_logFunc)), uncaught_on_entry_(other.uncaught_on_entry_) {
            other.m_logFunc = nullptr; // Nullify the moved-from object's function
        }
        LineStream& operator=(LineStream&& other) = delete;
    };
}
