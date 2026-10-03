#pragma once
#include "internal-logs.h"
#include <core/logging/logger.h>
#include <sstream>
#include <functional>
#include <exception>

// Configure the profile/mask target-wide (core/logging/compile-policy.h).
// Stream and guarded formatted macros filter before evaluating arguments.
// Direct trace/info/etc. function calls still evaluate their supplied arguments.
#define LOGLINESTREAM(log_level, logname) ctlog::LogLineStream([](const std::string& s) { CE::Logger<logname>::get().log_level("{}", s); })
#define CTLOG(ctl, log_level, logname)                                                                                                     \
    if constexpr (!ctlog::enabled(ctl)) {                                                                                                 \
    } else if (auto* ce_log_instance_ = ctlog::acquire_logger<logname>(static_cast<ctlog::LogLevel>(ctl)); !ce_log_instance_) {              \
    } else                                                                                                                              \
        ctlog::LogLineStream([ce_log_instance_](const std::string& s) { ce_log_instance_->log_level("{}", s); })

#define UTRACE(logname) CTLOG(ctlog::TRACE_, trace, logname)
#define UDEBUG(logname) CTLOG(ctlog::DEBUG_, debug, logname)
#define UINFO(logname) CTLOG(ctlog::INFO_, info, logname)
#define UWARN(logname) CTLOG(ctlog::WARNING_, warn, logname)
#define UERROR(logname) CTLOG(ctlog::ERROR_, error, logname)
#define UFATAL(logname) CTLOG(ctlog::FATAL_, critical, logname)

// The lambda keeps argument preparation inside the guarded emission boundary.
#define CE_LOG(ctl, log_level, logname, ...)                                                                                               \
    do {                                                                                                                                \
        CE::Logger<logname>::template write_lazy<ctl>([&](auto& ce_log_instance_) { ce_log_instance_.log_level(__VA_ARGS__); });             \
    } while (false)

#define CE_LOG_TRACE(logname, ...) CE_LOG(ctlog::TRACE_, trace, logname, __VA_ARGS__)
#define CE_LOG_DEBUG(logname, ...) CE_LOG(ctlog::DEBUG_, debug, logname, __VA_ARGS__)
#define CE_LOG_INFO(logname, ...) CE_LOG(ctlog::INFO_, info, logname, __VA_ARGS__)
#define CE_LOG_WARN(logname, ...) CE_LOG(ctlog::WARNING_, warn, logname, __VA_ARGS__)
#define CE_LOG_ERROR(logname, ...) CE_LOG(ctlog::ERROR_, error, logname, __VA_ARGS__)
#define CE_LOG_CRITICAL(logname, ...) CE_LOG(ctlog::FATAL_, critical, logname, __VA_ARGS__)

namespace ctlog {
    template <const char* name> [[nodiscard]] CE::Log<name>* acquire_logger(const LogLevel level) noexcept {
        if (CE::LogDetail::suppress_backend_emission())
            return nullptr;
        try {
            auto& log = CE::Logger<name>::get();
            return log.should_log(runtime_level(level)) ? &log : nullptr;
        } catch (...) {
            CE::Diagnostics::report_failure("stream logger acquisition", std::current_exception());
            return nullptr;
        }
    }

    struct LogLineStream : std::stringstream {
    protected:
        std::function<void(const std::string&)> m_logFunc; // Logging function
        const int uncaught_on_entry_;

    public:
        ~LogLineStream() override {
            // Do not emit a partial message while a failed argument/inserter is
            // unwinding. Submission/string-copy failures cannot escape destruction.
            if (!m_logFunc || std::uncaught_exceptions() > uncaught_on_entry_)
                return;
            try {
                m_logFunc(str());
            } catch (...) {
                CE::Diagnostics::report_failure("stream log emission", std::current_exception());
            }
        }

        // Constructor takes a logging function to be called on destruction
        explicit LogLineStream(std::function<void(const std::string&)> logFunc)
        : m_logFunc(std::move(logFunc)), uncaught_on_entry_(std::uncaught_exceptions()) {}

        // Move constructor
        LogLineStream(LogLineStream&& other)
        : std::stringstream(std::move(other)), m_logFunc(std::move(other.m_logFunc)), uncaught_on_entry_(other.uncaught_on_entry_) {
            other.m_logFunc = nullptr; // Nullify the moved-from object's function
        }
        LogLineStream& operator=(LogLineStream&& other) = delete;
    };
}
