#include <core/logging/log.h>

#include "backend-log.h"
#include "../../internal/stack-trace-internal.h"

#include <atomic>
#include <unordered_map>

namespace CE::Logging::Detail {
    std::mutex default_logger_mutex;

    uint16_t next_log_id() noexcept {
        static std::atomic_uint16_t counter{0};
        return static_cast<uint16_t>(counter.fetch_add(1, std::memory_order_relaxed) + 1);
    }

    namespace {
        spdlog::file_event_handlers native_handlers(FileEventHandlers handlers) {
            spdlog::file_event_handlers native;
            if (handlers.before_open)
                native.before_open = [callback = std::move(handlers.before_open)](const spdlog::filename_t& file) {
                    callback(std::filesystem::path(file));
                };
            if (handlers.after_open)
                native.after_open = [callback = std::move(handlers.after_open)](const spdlog::filename_t& file, std::FILE* stream) {
                    callback(std::filesystem::path(file), stream);
                };
            if (handlers.before_close)
                native.before_close = [callback = std::move(handlers.before_close)](const spdlog::filename_t& file, std::FILE* stream) {
                    callback(std::filesystem::path(file), stream);
                };
            if (handlers.after_close)
                native.after_close = [callback = std::move(handlers.after_close)](const spdlog::filename_t& file) {
                    callback(std::filesystem::path(file));
                };
            return native;
        }
    }

    struct LogState {
        BackendLog backend;

        LogState(const char* name, FileEventHandlers handlers, Config config)
        : backend(name ? name : "", native_handlers(std::move(handlers)), std::move(config)) {}
    };

    namespace {
        class CategoryHandle final : public LogHandle {
        public:
            explicit CategoryHandle(std::shared_ptr<LogState> state)
            : LogHandle(std::move(state)) {}
        };

        struct Category {
            std::mutex mutex;
            std::condition_variable ready;
            std::shared_ptr<LogState> state;
            std::unique_ptr<CategoryHandle> handle;
            std::atomic<LogHandle*> published{nullptr};
            bool creating{};

            ~Category() { published.store(nullptr, std::memory_order_release); }
        };

        struct Categories;
        constinit std::atomic<Categories*> published_categories{nullptr};

        struct Categories {
            std::mutex mutex;
            std::unordered_map<std::string, std::shared_ptr<Category>> entries;

            Categories() { published_categories.store(this, std::memory_order_release); }
            ~Categories() { published_categories.store(nullptr, std::memory_order_release); }
        };

        Categories& categories() {
            // Backends release native registrations when the category map dies.
            // Construct their registry first so it survives that final release.
            static_cast<void>(spdlog::default_logger());
            static Categories registry;
            return registry;
        }
    }

    std::shared_ptr<LogState> acquire_category(const char* name, FileEventHandlers handlers, Config config, const bool initialize) {
        reject_backend_reentry(initialize ? "initialize a logger" : "acquire a logger");
        static_cast<void>(spdlog::default_logger());
        auto& registry = categories();
        std::shared_ptr<Category> category;
        {
            const std::lock_guard lock(registry.mutex);
            auto& entry = registry.entries[name ? name : ""];
            if (!entry)
                entry = std::make_shared<Category>();
            category = entry;
        }
        std::unique_lock lock(category->mutex);
        category->ready.wait(lock, [&] { return !category->creating; });
        if (category->state) {
            if (initialize)
                throw Exceptions::bad_request(CE_HERE, std::format("The '{}' logger is already initialized.", name));
            return category->state;
        }
        category->creating = true;
        lock.unlock();
        try {
            auto state = std::make_shared<LogState>(name, std::move(handlers), std::move(config));
            auto handle = std::make_unique<CategoryHandle>(state);
            lock.lock();
            category->state = state;
            category->handle = std::move(handle);
            category->published.store(category->handle.get(), std::memory_order_release);
            category->creating = false;
            lock.unlock();
            category->ready.notify_all();
            return state;
        } catch (...) {
            lock.lock();
            category->creating = false;
            lock.unlock();
            category->ready.notify_all();
            throw;
        }
    }

    LogHandle* existing_category(const char* name) noexcept {
        try {
            auto* registry = published_categories.load(std::memory_order_acquire);
            if (!registry)
                return nullptr;
            const std::unique_lock lock(registry->mutex, std::try_to_lock);
            if (!lock.owns_lock())
                return nullptr;
            const std::string_view category_name = name ? name : "";
            for (const auto& [key, category] : registry->entries)
                if (key == category_name)
                    return category->published.load(std::memory_order_acquire);
        } catch (...) {}
        return nullptr;
    }
}

namespace CE::Logging {
    LogHandle::LogHandle(const char* name, FileEventHandlers handlers, Config config) {
        Detail::reject_backend_reentry("construct a logger");
        static_cast<void>(spdlog::default_logger());
        state_ = std::make_shared<Detail::LogState>(name, std::move(handlers), std::move(config));
    }

    LogHandle::LogHandle(std::shared_ptr<Detail::LogState> state)
    : state_(std::move(state)) {}
    LogHandle::~LogHandle() = default;

    const Config& LogHandle::initial_configuration() const noexcept {
        return state_->backend.initial_configuration();
    }
    QueueStats LogHandle::shared_queue_stats() const {
        return state_->backend.shared_queue_stats();
    }
    BackendStats LogHandle::backend_stats() const noexcept {
        return state_->backend.backend_stats();
    }
    void LogHandle::report_diagnostics() const {
        state_->backend.report_diagnostics();
    }
    std::filesystem::path LogHandle::get_file_path() const {
        return state_->backend.get_file_path();
    }
    std::uint16_t LogHandle::get_log_id() const {
        return state_->backend.get_log_id();
    }
    void LogHandle::flush() const {
        state_->backend.flush();
    }
    void LogHandle::close(const std::chrono::milliseconds timeout) {
        state_->backend.close(timeout);
    }
    void LogHandle::reopen() {
        state_->backend.reopen();
    }
    void LogHandle::make_default() const {
        state_->backend.make_default();
    }
    void LogHandle::set_level_logger(const Level level) const {
        state_->backend.set_level_logger(static_cast<spdlog::level>(level));
    }
    void LogHandle::set_level_filesink(const Level level) const {
        state_->backend.set_level_filesink(static_cast<spdlog::level>(level));
    }
    void LogHandle::set_level_stdsink(const Level level) const {
        state_->backend.set_level_stdsink(static_cast<spdlog::level>(level));
    }
    bool LogHandle::should_log(const Level level) const noexcept {
        return state_->backend.should_log(static_cast<spdlog::level>(level));
    }
    void LogHandle::set_pattern(const char* format) const {
        state_->backend.set_pattern(format);
    }
    void LogHandle::set_pattern_filesink(const char* format) const {
        state_->backend.set_pattern_filesink(format);
    }
    void LogHandle::set_pattern_stdsink(const char* format) const {
        state_->backend.set_pattern_stdsink(format);
    }
    void LogHandle::strace(void* address) const {
        state_->backend.strace(address);
    }
    void LogHandle::submit(const Level level, const std::string_view message, const bool trace) const {
        state_->backend.submit(static_cast<spdlog::level>(level), message, trace);
    }
}

namespace CE::Diagnostics {
    std::string stack_trace(void* address) noexcept {
        try {
            return Detail::capture<6144>(address, address ? 7 : 17, 0);
        } catch (...) {
            return {};
        }
    }
}

namespace CE::State {
    template <> auto Singleton<CE::Logging::Detail::ThreadPoolInit>::storage() -> Storage& {
        static Storage value;
        return value;
    }
}
