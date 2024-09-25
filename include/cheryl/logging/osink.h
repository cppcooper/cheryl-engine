#pragma once
#include <spdlog/common.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>

#include <iostream>
#include <mutex>

namespace CE {
    template<typename Mutex>
    class osink : public spdlog::sinks::base_sink<Mutex> {
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
            if (msg.log_level < spdlog::level::err) {
                std::cout<<formatted;
            } else {
                std::cerr<<formatted;
            }
        }

        void flush_() override {
            std::cout.flush();
            std::cerr.flush();
        }
    };

    using osink_mt = osink<std::mutex>;
    using osink_st = osink<spdlog::details::null_mutex>;
}
