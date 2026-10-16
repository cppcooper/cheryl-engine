#include <core/logging/process-lifetime.h>
#include <core/logging/log.h>
#include <internals/failure-reporting.h>

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

namespace CE::Logging::Detail {
    namespace {
        struct ProcessState {
            std::mutex mutex;
            std::vector<std::weak_ptr<ProcessLogRegistration>> logs;
            std::size_t scopes = 0;
            bool stopped = false;
        };

        ProcessState& process_state() {
            static ProcessState state;
            return state;
        }

        void acquire_process_scope() {
            reject_backend_reentry("begin the process logging lifetime");
            auto& state = process_state();
            const std::lock_guard lock(state.mutex);
            if (state.stopped)
                throw ::CE::Exceptions::failed_operation(CE_HERE, "The process logging lifetime has ended");
            ++state.scopes;
        }

        void release_process_scope() {
            if (in_backend()) {
                ::CE::Diagnostics::report_failure("process logging shutdown from a backend callback", {});
                std::terminate();
            }
            std::vector<std::weak_ptr<ProcessLogRegistration>> logs;
            {
                auto& state = process_state();
                const std::lock_guard lock(state.mutex);
                if (--state.scopes != 0)
                    return;
                state.stopped = true;
                logs.swap(state.logs);
            }
            // Closing a destination can invoke user handlers. Never hold the
            // process registry lock while waiting for those callbacks/workers.
            for (const auto& weak : logs) {
                if (const auto registration = weak.lock()) {
                    const std::lock_guard lock(registration->mutex);
                    if (registration->shutdown)
                        registration->shutdown();
                }
            }
            if (auto* pool = ThreadPoolInit::get_existing())
                pool->tp.reset();
        }
    }

    std::shared_ptr<ProcessLogRegistration> register_process_log(std::function<void()> shutdown) {
        auto registration = std::make_shared<ProcessLogRegistration>();
        registration->shutdown = std::move(shutdown);
        auto& state = process_state();
        const std::lock_guard lock(state.mutex);
        if (state.stopped)
            throw ::CE::Exceptions::failed_operation(CE_HERE, "The process logging lifetime has ended");
        std::erase_if(state.logs, [](const auto& entry) { return entry.expired(); });
        state.logs.push_back(registration);
        return registration;
    }
}

namespace CE::Logging {
    ProcessLifetime::ProcessLifetime() {
        Detail::acquire_process_scope();
    }

    ProcessLifetime::~ProcessLifetime() noexcept {
        try {
            Detail::release_process_scope();
        } catch (...) {
            ::CE::Diagnostics::report_failure("process logging shutdown", std::current_exception());
            std::terminate();
        }
    }
}
