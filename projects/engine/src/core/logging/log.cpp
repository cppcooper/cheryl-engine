#include <core/logging/log.h>
#include <internals/stack-trace-internal.h>

#include <atomic>

namespace CE::Logging::Detail {
    std::mutex default_logger_mutex;
    uint16_t next_log_id() noexcept {
        // This legacy diagnostic serial wraps; it is not a session/domain identity.
        // It publishes no other logger state, so relaxed ordering is sufficient.
        static std::atomic_uint16_t log_counter{0};
        return static_cast<uint16_t>(log_counter.fetch_add(1, std::memory_order_relaxed) + 1);
    }
}

namespace CE::Diagnostics {
    std::string stack_trace(void* addr0) noexcept {
        try {
            return Detail::capture<6144>(addr0, addr0 ? 7 : 17, 0);
        } catch (...) {
            // Default construction and moving an empty string require no allocation.
            // Ordinary logging decides how to display unavailable trace information.
            return {};
        }
    }
}

namespace CE::State {
    template <> auto Singleton<CE::Logging::Detail::ThreadPoolInit>::storage() -> Storage& {
        static Storage value;
        return value;
    }
    template <> auto Singleton<CE::Logging::Log<CE::Logging::Categories::engine>>::storage() -> Storage& {
        static Storage value;
        return value;
    }
    template <> auto Singleton<CE::Logging::Log<CE::Logging::Categories::platform>>::storage() -> Storage& {
        static Storage value;
        return value;
    }
    template <> auto Singleton<CE::Logging::Log<CE::Logging::Categories::rendering>>::storage() -> Storage& {
        static Storage value;
        return value;
    }
    template <> auto Singleton<CE::Logging::Log<CE::Logging::Categories::assets>>::storage() -> Storage& {
        static Storage value;
        return value;
    }
    template <> auto Singleton<CE::Logging::Log<CE::Logging::Categories::memory>>::storage() -> Storage& {
        static Storage value;
        return value;
    }
    template <> auto Singleton<CE::Logging::Log<CE::Logging::Categories::cheryl>>::storage() -> Storage& {
        static Storage value;
        return value;
    }
}
