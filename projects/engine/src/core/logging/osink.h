#pragma once
#include <spdlog/common.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>

#include <iostream>
#include <mutex>

namespace CE::Logging::Sinks {
    template <typename Mutex> class OutputSink : public spdlog::sinks::base_sink<Mutex> {
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
            if (msg.log_level < spdlog::level::err) {
                std::cout << formatted;
            } else {
                std::cerr << formatted;
            }
        }

        void flush_() override {
            std::cout.flush();
            std::cerr.flush();
        }
    };

    using ThreadSafeOutputSink = OutputSink<std::mutex>;
    using SingleThreadOutputSink = OutputSink<spdlog::details::null_mutex>;
}
