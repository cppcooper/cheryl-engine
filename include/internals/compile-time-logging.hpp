#pragma once
#ifndef COMPILE_TIME_LOGGING_HPP
#define COMPILE_TIME_LOGGING_HPP
#include "internal-logs.h"
#include <sstream>
#include <functional>

/* Instructions:
 * use `#define CTWriteMask <value>`
 *
 * This will configure the maximum log levels to write.
 *
 * usage example:
 * LOG(FATAL_,critical,"mylog") << "something critical happened and we cannot recover. " << obj;
 */

#define CTWriteMask ((ctlog::LogLevel::TRACE_<<1)-1)
#define LOGLINESTREAM(log_level,logname)   ctlog::LogLineStream([](const std::string& s){CE::Logger<logname>::get().log_level("{}", s);})
#define CTLOG(ctl,log_level,logname)    if constexpr (ctl & CTWriteMask) LOGLINESTREAM(log_level,logname)

#define UTRACE(logname) CTLOG(ctlog::TRACE_,trace,logname)
#define UDEBUG(logname) CTLOG(ctlog::DEBUG_,debug,logname)
#define UINFO(logname) CTLOG(ctlog::INFO_,info,logname)
#define UWARN(logname) CTLOG(ctlog::WARNING_,warn,logname)
#define UERROR(logname) CTLOG(ctlog::ERROR_,error,logname)
#define UFATAL(logname) CTLOG(ctlog::FATAL_,critical,logname)

namespace ctlog {
    enum LogLevel {
        FATAL_   = 1 << 0,
        ERROR_   = 1 << 1,
        WARNING_ = 1 << 2,
        INFO_   = 1 << 3,
        DEBUG_  = 1 << 4,
        TRACE_ = 1 << 5
    };

    struct LogLineStream : std::stringstream {
    protected:
        std::function<void(const std::string&)> m_logFunc;  // Logging function
    public:
        ~LogLineStream() override { if (m_logFunc) m_logFunc(str()); }

        // Constructor takes a logging function to be called on destruction
        explicit LogLineStream(std::function<void(const std::string&)> logFunc)
            : m_logFunc(std::move(logFunc)) {}

        // Move constructor
        LogLineStream(LogLineStream&& other) noexcept
            : std::stringstream(std::move(other)), m_logFunc(std::move(other.m_logFunc)) {
            other.m_logFunc = nullptr;  // Nullify the moved-from object's function
        }
        LogLineStream& operator=(LogLineStream&& other) = delete;
    };
}

#endif //COMPILE_TIME_LOGGING_HPP
